#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/system_properties.h>
#include <android/log.h>
#include <fcntl.h>
#include <csignal>
#include <unordered_set>
#include "SelinuxCompat.hpp"

#define LOG_TAG "GameUnlocker/Daemon"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define SOCKET_NAME "@gameunlocker_daemon"
#define MAX_SYSFS_PATH 256
#define MAX_WHITELISTED_UIDS 64

static std::atomic<int>   g_active_clients(0);

static std::unordered_set<uid_t> g_whitelisted_uids;

static std::mutex         g_uid_mutex;

static std::string g_saved_gpu_mode;

static std::string g_saved_gfx_quality;

static std::string g_saved_kgsl_governor;

static std::string getProp(const char* name) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get(name, value);
    return std::string(value);
}

static void setPropSafe(const char* name, const char* value) {
    if (__system_property_set(name, value) != 0) {
        LOGW("__system_property_set failed for %s", name);
    }
}

static bool isQualcomm() {

    std::string hw = getProp("ro.hardware");

    static const char* const QTI_HW[] = {
        "qcom", "kalama", "taro", "lahaina", "shima", "crow",
        "cape", "uksi", "napa", "pineapple", "sun", nullptr
    };
    for (int i = 0; QTI_HW[i]; ++i) {
        if (hw.find(QTI_HW[i]) != std::string::npos) return true;
    }

    std::string board = getProp("ro.board.platform");
    if (!board.empty() && (board[0] == 's' || board.find("msm") == 0 ||
                            board.find("qcom") == 0)) {
        return true;
    }
    return false;
}

static std::string findKgslPath() {

    static const char* const KGSL_PATHS[] = {
        "/sys/class/kgsl/kgsl-3d0",
        "/sys/kernel/gpu/gpu0",
        nullptr
    };
    for (int i = 0; KGSL_PATHS[i]; ++i) {
        struct stat st{};
        if (stat(KGSL_PATHS[i], &st) == 0) return KGSL_PATHS[i];
    }
    return {};
}

static void applyPerfMode() {
    LOGI("Applying performance mode (active_clients=%d)", g_active_clients.load());
    if (isQualcomm()) {
        setPropSafe("vendor.gpu.mode",         "performance");
        setPropSafe("vendor.gfx.low_quality",  "1");
        setPropSafe("vendor.thermal.gaming.mode", "1");

        std::string kgsl = findKgslPath();
        if (!kgsl.empty()) {
            char gov_buf[64] = {0};

            std::string gov_path = kgsl + "/devfreq/governor";
            if (gameunlocker::selinux::safe_sysfs_read(gov_path.c_str(), gov_buf, sizeof(gov_buf))) {
                g_saved_kgsl_governor = gov_buf;
            }
            auto result = gameunlocker::selinux::safe_sysfs_write(gov_path.c_str(), "performance");
            if (result == gameunlocker::selinux::SysfsWriteResult::DENIED) {
                LOGW("Cannot set Adreno governor (SELinux). Using props fallback.");
                setPropSafe("debug.adreno.gov", "performance");
            }

            std::string max_freq_path = kgsl + "/devfreq/max_freq";
            char max_freq_buf[32] = {0};
            if (gameunlocker::selinux::safe_sysfs_read(max_freq_path.c_str(), max_freq_buf, sizeof(max_freq_buf))) {
                long max_freq = std::strtol(max_freq_buf, nullptr, 10);
                if (max_freq > 0) {

                    std::string min_freq_path = kgsl + "/devfreq/min_freq";
                    gameunlocker::selinux::safe_sysfs_write_int(min_freq_path.c_str(), max_freq / 2);
                }
            }
            gameunlocker::selinux::safe_sysfs_write((kgsl + "/idle_timer").c_str(), "80");
            LOGI("Adreno performance mode applied (%s)", kgsl.c_str());
        }
    }
    for (int i = 0; i < 8; ++i) {
        char path[128];
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/up_rate_limit_us", i);
        gameunlocker::selinux::safe_sysfs_write(path, "500");
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/down_rate_limit_us", i);
        gameunlocker::selinux::safe_sysfs_write(path, "20000");
    }
    gameunlocker::selinux::safe_sysfs_write("/proc/sys/vm/swappiness", "5");
}

static void restorePerfMode() {
    LOGI("Restoring baseline (active_clients=%d)", g_active_clients.load());
    if (isQualcomm()) {
        setPropSafe("vendor.gpu.mode",          g_saved_gpu_mode.c_str());
        setPropSafe("vendor.gfx.low_quality",   g_saved_gfx_quality.c_str());
        setPropSafe("vendor.thermal.gaming.mode", "0");

        std::string kgsl = findKgslPath();
        if (!kgsl.empty() && !g_saved_kgsl_governor.empty()) {
            gameunlocker::selinux::safe_sysfs_write(
                (kgsl + "/devfreq/governor").c_str(),
                g_saved_kgsl_governor.c_str()
            );

            std::string avail_path = kgsl + "/devfreq/available_frequencies";
            char avail_buf[512] = {0};
            if (gameunlocker::selinux::safe_sysfs_read(avail_path.c_str(), avail_buf, sizeof(avail_buf))) {
                long min_freq = 0;
                char* tok = std::strtok(avail_buf, " \t\n");
                while (tok) {
                    long f = std::strtol(tok, nullptr, 10);
                    if (f > 0 && (min_freq == 0 || f < min_freq)) min_freq = f;
                    tok = std::strtok(nullptr, " \t\n");
                }
                if (min_freq > 0) {
                    gameunlocker::selinux::safe_sysfs_write_int(
                        (kgsl + "/devfreq/min_freq").c_str(), min_freq
                    );
                }
            }
        }
    }
    for (int i = 0; i < 8; ++i) {
        char path[128];
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/up_rate_limit_us", i);
        gameunlocker::selinux::safe_sysfs_write(path, "10000");  
        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/down_rate_limit_us", i);
        gameunlocker::selinux::safe_sysfs_write(path, "40000");  
    }
    gameunlocker::selinux::safe_sysfs_write("/proc/sys/vm/swappiness", "10");
}

static void handleClient(int client_fd) {
    struct ucred ucred{};
    socklen_t ucred_len = sizeof(ucred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_len) < 0) {
        LOGE("getsockopt SO_PEERCRED failed: %s", strerror(errno));
        close(client_fd);
        return;
    }
    char buf[512];
    ssize_t bytes = read(client_fd, buf, sizeof(buf) - 1);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    buf[bytes] = '\0';
    if (bytes > 0 && buf[bytes - 1] == '\n') buf[bytes - 1] = '\0';

    std::string command(buf);
    if (command.rfind("WHITELIST:", 0) == 0) {
        if (ucred.uid != 0) {
            LOGW("Non-root (uid %d) attempted WHITELIST — rejected", ucred.uid);
            close(client_fd);
            return;
        }
        try {
            uid_t target_uid = static_cast<uid_t>(std::stoi(command.substr(10)));

            std::lock_guard<std::mutex> lock(g_uid_mutex);
            if (g_whitelisted_uids.size() < MAX_WHITELISTED_UIDS) {
                g_whitelisted_uids.insert(target_uid);
                LOGI("Whitelisted UID: %u", (unsigned)target_uid);
            } else {
                LOGW("Whitelist at capacity (%d), rejecting UID %u",
                     MAX_WHITELISTED_UIDS, (unsigned)target_uid);
            }
        } catch (...) {
            LOGE("Invalid UID in WHITELIST command: %s", command.c_str());
        }
        close(client_fd);
        return;
    }
    if (command.rfind("SYSFS:", 0) == 0) {

        bool allowed = (ucred.uid == 0);
        if (!allowed) {

            std::lock_guard<std::mutex> lock(g_uid_mutex);
            allowed = g_whitelisted_uids.count(ucred.uid) > 0;
        }
        if (!allowed) {
            LOGW("Unauthorized SYSFS write from uid %u", (unsigned)ucred.uid);
            close(client_fd);
            return;
        }

        std::string payload = command.substr(6);
        size_t sep = payload.find('|');
        if (sep != std::string::npos && sep > 0) {

            std::string path  = payload.substr(0, sep);

            std::string value = payload.substr(sep + 1);
            if (path.length() < MAX_SYSFS_PATH && path[0] == '/') {
                auto result = gameunlocker::selinux::safe_sysfs_write(path.c_str(), value.c_str());
                if (result == gameunlocker::selinux::SysfsWriteResult::DENIED) {
                    LOGW("Daemon: SELinux denial for delegated write %s", path.c_str());
                }
            } else {
                LOGW("Invalid sysfs path in SYSFS command: %s", path.c_str());
            }
        }
        close(client_fd);
        return;
    }
    if (command == "CONNECT") {

        bool allowed = (ucred.uid == 0);
        if (!allowed) {

            std::lock_guard<std::mutex> lock(g_uid_mutex);
            allowed = g_whitelisted_uids.count(ucred.uid) > 0;
        }
        if (!allowed) {
            LOGW("Unauthorized CONNECT from uid %u", (unsigned)ucred.uid);
            close(client_fd);
            return;
        }
        if (g_active_clients.fetch_add(1) == 0) {
            applyPerfMode();
        }
        LOGI("Game connected (uid=%u). Active sessions: %d",
             (unsigned)ucred.uid, g_active_clients.load());
        char ping[16];
        while (read(client_fd, ping, sizeof(ping)) > 0) {}

        int remaining = g_active_clients.fetch_sub(1) - 1;
        LOGI("Game disconnected (uid=%u). Remaining sessions: %d",
             (unsigned)ucred.uid, remaining);
        if (remaining == 0) {
            restorePerfMode();
        }
        close(client_fd);
        return;
    }
    LOGW("Unknown command from uid=%u: %s", (unsigned)ucred.uid, command.c_str());
    close(client_fd);
}

int main() {
    LOGI("GameUnlocker daemon starting (pid=%d)", getpid());
    g_saved_gpu_mode     = getProp("vendor.gpu.mode");
    g_saved_gfx_quality  = getProp("vendor.gfx.low_quality");

    int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd < 0) {
        LOGE("socket() failed: %s", strerror(errno));
        return 1;
    }
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';  
    strncpy(addr.sun_path + 1, &SOCKET_NAME[1], sizeof(addr.sun_path) - 2);
    const int addr_len = static_cast<int>(
        offsetof(struct sockaddr_un, sun_path) + strlen(&SOCKET_NAME[1]) + 1
    );

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
        LOGE("bind() failed: %s", strerror(errno));
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, 16) < 0) {
        LOGE("listen() failed: %s", strerror(errno));
        close(server_fd);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    LOGI("Listening on abstract socket: %s", SOCKET_NAME);
    while (true) {

        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {

            std::thread(handleClient, client_fd).detach();
        } else if (errno != EINTR) {
            LOGE("accept() error: %s", strerror(errno));
        }
    }
    close(server_fd);
    return 0;
}