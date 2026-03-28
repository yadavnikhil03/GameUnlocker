#pragma once
#include <jni.h>
namespace zygisk {
struct Api {
    virtual void* get() = 0;
    virtual int connectCompanion(int fd = -1) = 0;
    virtual void setOption(int opt) = 0;
};
struct AppSpecializeArgs {
    jint& uid;
    jint& gid;
    jintArray& gids;
    jint& runtime_flags;
    jobjectArray& rlimits;
    jint& mount_external;
    jstring& se_info;
    jstring& nice_name;
    jstring& instruction_set;
    jstring& app_data_dir;
};
struct ServerSpecializeArgs {
    jint& uid;
    jint& gid;
    jintArray& gids;
    jint& runtime_flags;
    jlong& permitted_capabilities;
    jlong& effective_capabilities;
    jint& mount_external;
    jstring& se_info;
    jstring& nice_name;
    jboolean& is_child_zygote;
    jstring& instruction_set;
    jstring& app_data_dir;
};
class ModuleBase {
public:
    virtual ~ModuleBase() = default;
    virtual void onLoad(Api* /*api*/, JNIEnv* /*env*/) {}
    virtual void preAppSpecialize(AppSpecializeArgs* /*args*/) {}
    virtual void postAppSpecialize(const AppSpecializeArgs* /*args*/) {}
    virtual void preServerSpecialize(ServerSpecializeArgs* /*args*/) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs* /*args*/) {}
};
enum Option : int {
    FORCE_DENYLIST_UNMOUNT = 0,
    DLCLOSE_MODULE_LIBRARY = 1,
};
}

#define REGISTER_ZYGISK_MODULE(clazz) \
void zygisk_module_entry(zygisk::Api* api, JNIEnv* env) { \
    auto* module = new clazz(); \
    module->onLoad(api, env); \
}
#define REGISTER_ZYGISK_COMPANION(func) \
void zygisk_companion_entry(int fd) { \
    func(fd); \
}
