#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include <fcntl.h>
#include <csignal>
#include <unordered_set>

#define LOG_TAG "GameUnlockerDaemon"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define SOCKET_NAME "@gameunlocker_daemon"

std::atomic<int> active_clients(0);
std::string default_gpu_mode = "";
std::string default_gfx_low_quality = "";
std::unordered_set<uid_t> whitelisted_uids;
std::mutex uid_mutex;

std::string getProp(const char* name) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get(name, value);
    return std::string(value);
}

void setProp(const char* name, const char* value) {
    std::string cmd = std::string("setprop ") + name + " '" + value + "'";
    system(cmd.c_str());
}

bool isQualcomm() {
    std::string hw = getProp("ro.hardware");
    if (hw == "qcom" || hw.find("kalama") == 0 || hw.find("taro") == 0 || hw.find("lahaina") == 0 || 
        hw.find("shima") == 0 || hw.find("napa") == 0 || hw.find("napali") == 0 || 
        hw.find("crow") == 0 || hw.find("cape") == 0 || hw.find("uksi") == 0) {
        return true;
    }
    return false;
}

void applyPerfMode() {
    if (!isQualcomm()) return;
    LOGI("Applying Performance Mode");
    setProp("vendor.gpu.mode", "performance");
    setProp("vendor.gfx.low_quality", "1");
}

void restorePerfMode() {
    if (!isQualcomm()) return;
    LOGI("Restoring Normal Mode");
    setProp("vendor.gpu.mode", default_gpu_mode.c_str());
    setProp("vendor.gfx.low_quality", default_gfx_low_quality.c_str());
}

void handleClient(int client_fd) {
    struct ucred ucred;
    socklen_t len = sizeof(struct ucred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) < 0) {
        close(client_fd);
        return;
    }

    char buf[256];
    ssize_t bytes = read(client_fd, buf, sizeof(buf) - 1);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buf[bytes] = '\0';
    std::string command(buf);

    if (command.rfind("WHITELIST:", 0) == 0) {
        if (ucred.uid != 0) {
            LOGW("Non-root (uid %d) attempted to whitelist", ucred.uid);
            close(client_fd);
            return;
        }
        uid_t target_uid = std::stoi(command.substr(10));
        std::lock_guard<std::mutex> lock(uid_mutex);
        whitelisted_uids.insert(target_uid);
        LOGI("Whitelisted UID: %d", target_uid);
        close(client_fd);
        return;
    }

    if (command == "CONNECT") {
        bool allowed = false;
        {
            std::lock_guard<std::mutex> lock(uid_mutex);
            allowed = whitelisted_uids.count(ucred.uid) > 0;
        }

        if (!allowed && ucred.uid != 0) {
            LOGW("Unauthorized process (uid %d) attempted to trigger perf mode", ucred.uid);
            close(client_fd);
            return;
        }

        if (active_clients.fetch_add(1) == 0) {
            applyPerfMode();
        }
        LOGI("Client connected (uid %d). Active clients: %d", ucred.uid, active_clients.load());

        char ping[16];
        while (read(client_fd, ping, sizeof(ping)) > 0) {}

        if (active_clients.fetch_sub(1) == 1) {
            restorePerfMode();
        }
        LOGI("Client disconnected (uid %d). Active clients: %d", ucred.uid, active_clients.load());
        close(client_fd);
        return;
    }

    close(client_fd);
}

int main() {
    LOGI("GameUnlocker Daemon starting...");

    default_gpu_mode = getProp("vendor.gpu.mode");
    default_gfx_low_quality = getProp("vendor.gfx.low_quality");

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOGE("Failed to create socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, &SOCKET_NAME[1], sizeof(addr.sun_path) - 2);

    int len = offsetof(struct sockaddr_un, sun_path) + strlen(&SOCKET_NAME[1]) + 1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&addr, len) < 0) {
        LOGE("Failed to bind socket");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        LOGE("Failed to listen on socket");
        close(server_fd);
        return 1;
    }

    LOGI("Listening on abstract socket: %s", SOCKET_NAME);
    signal(SIGPIPE, SIG_IGN);

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            std::thread(handleClient, client_fd).detach();
        }
    }

    close(server_fd);
    return 0;
}
