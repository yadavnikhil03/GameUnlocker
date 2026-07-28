#include "Companion.hpp"
#include "logger.hpp"
#include "raii.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace gameunlocker {

CompanionManager::CompanionManager(const Context& ctx) : ctx_(ctx) {}

std::string CompanionManager::resolveModulePath() const {
    FdWrapper dirfd(ctx_.getModuleDirFd());
    if (!dirfd.isValid()) return "";

    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", dirfd.get());

    char modulePath[PATH_MAX];
    ssize_t len = readlink(fdPath, modulePath, sizeof(modulePath) - 1);
    if (len <= 0) return "";

    modulePath[len] = '\0';
    return std::string(modulePath);
}

bool CompanionManager::mountCpuInfo(const std::string& modulePath, const std::string& hardware) const {
    if (modulePath.empty()) return false;
    std::string spoofPath = modulePath + "/cpuinfo_spoof";
    return executeCompanionCommand("mount_spoof:" + hardware + "|" + spoofPath);
}

bool CompanionManager::unmountCpuInfo() const {
    return executeCompanionCommand("unmount_spoof");
}

bool CompanionManager::whitelistDaemon(uid_t targetUid) const {
    return executeCompanionCommand("whitelist_daemon:" + std::to_string(targetUid));
}

bool CompanionManager::executeCompanionCommand(const std::string& command) const {
    if (!ctx_.getApi()) return false;

    FdWrapper fd_conn(ctx_.getApi()->connectCompanion());
    if (!fd_conn.isValid()) {
        LOGW("CompanionManager: Failed to connect to companion");
        return false;
    }
    ssize_t written = write(fd_conn.get(), command.c_str(), command.size());
    return written == static_cast<ssize_t>(command.size());
}

// ============================================================
// Companion handler — runs as ROOT in the zygote companion
// process. Performs privileged bind-mount of cpuinfo_spoof.
// ============================================================
void companionHandler(int fd) {
    char buffer[512];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) return;
    buffer[bytes] = '\0';

    std::string command(buffer, static_cast<size_t>(bytes));

    if (command == "unmount_spoof") {
        // Best-effort unmount; ignore errors (may not be mounted)
        umount2("/proc/cpuinfo", MNT_DETACH);
        LOGI("Companion: Unmounted /proc/cpuinfo");
        return;
    }

    if (command.rfind("mount_spoof:", 0) == 0) {
        std::string payload = command.substr(12);
        size_t pipe_pos = payload.find('|');
        if (pipe_pos == std::string::npos) return;
        
        std::string hardware = payload.substr(0, pipe_pos);
        std::string spoofPath = payload.substr(pipe_pos + 1);

        if (spoofPath.rfind("/data/adb/modules/", 0) != 0) {
            LOGW("Companion: Rejected path outside module dir: %s", spoofPath.c_str());
            return;
        }

        FILE* fp = fopen(spoofPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "Processor\t: AArch64 Processor rev 0 (aarch64)\n");
            fprintf(fp, "system type\t: %s\n", hardware.c_str());
            fprintf(fp, "Hardware\t: %s\n", hardware.c_str());
            for (int i = 0; i < 8; i++) {
                fprintf(fp, "\nprocessor\t: %d\n", i);
                fprintf(fp, "BogoMIPS\t: 38.40\n");
                fprintf(fp, "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32\n");
                fprintf(fp, "CPU implementer\t: 0x41\n");
                fprintf(fp, "CPU architecture: 8\n");
                fprintf(fp, "CPU variant\t: 0x1\n");
                fprintf(fp, "CPU part\t: 0x000\n");
                fprintf(fp, "CPU revision\t: 0\n");
            }
            fclose(fp);
            chmod(spoofPath.c_str(), 0644);
        } else {
            LOGW("Companion: Failed to generate cpuinfo at: %s", spoofPath.c_str());
            return;
        }

        umount2("/proc/cpuinfo", MNT_DETACH);
        int ret = mount(spoofPath.c_str(), "/proc/cpuinfo", nullptr, MS_BIND, nullptr);
        if (ret == 0) {
            LOGI("Companion: Mounted dynamic cpuinfo (%s) -> /proc/cpuinfo", hardware.c_str());
        } else {
            LOGW("Companion: mount --bind failed (exit=%d)", ret);
        }
        return;
    }

    if (command.rfind("whitelist_daemon:", 0) == 0) {
        std::string uidStr = command.substr(17);
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            const char* socket_name = "@gameunlocker_daemon";
            addr.sun_path[0] = '\0';
            strncpy(addr.sun_path + 1, socket_name + 1, sizeof(addr.sun_path) - 2);
            int len = offsetof(struct sockaddr_un, sun_path) + strlen(socket_name + 1) + 1;
            
            if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), len) == 0) {
                std::string wlCmd = "WHITELIST:" + uidStr;
                write(sock, wlCmd.c_str(), wlCmd.size());
                LOGI("Companion: Sent whitelist command for uid %s", uidStr.c_str());
            } else {
                LOGW("Companion: Failed to connect to controller daemon to whitelist uid %s", uidStr.c_str());
            }
            close(sock);
        }
        return;
    }

    LOGW("Companion: Unknown command: %s", command.c_str());
}

} // namespace gameunlocker
