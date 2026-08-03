# Changelog

## v2.2.0-rc1 (Pre-Release)

### Major Fix
- **Fixed WebUI completely broken in KernelSU/MMRL**: KSUWebUI does not support CGI script execution — all `fetch('cgi-bin/api.sh')` calls were returning raw shell script text instead of JSON. Implemented a **dual-mode API layer** that auto-detects the environment:
  - **KSUWebUI mode**: Uses `ksu.exec()` JavaScript bridge to execute shell commands natively with root privileges
  - **Magisk browser mode**: Falls back to `fetch()` CGI for backward compatibility
  - All 11 API actions (get_config, device_info, add/remove game, profiles, logs, cloud update, backup/restore) are fully mapped

### Security Improvements
- Added authentication token enforcement for WebUI API in browser mode
- Replaced `strcpy` with `strncpy` + null-termination in SysPropHook for buffer safety
- Added config parsing failure logging for easier debugging

### Bug Fixes
- Fixed stale `auth_token` file persisting after module update causing API lockout
- Fixed uninstall cleanup missing `gameunlocker_apps.json` temp file
- Improved error display in WebUI — shows specific error messages instead of silent failures

## v2.1.1

### New Features
- **Magisk Companion App**: The WebUI has been completely overhauled with a modern, high-fidelity Material Design 3 aesthetic. For Magisk users, the module now automatically installs a native Android Companion App (`GameUnlockerUI`) in your launcher. This app utilizes `libsu` for robust, native root shell execution without deadlocks on large outputs. (Resolves #11)

### Bug Fixes
- **Fixed spoofing bypass on Android 14+ native games (Resolves #14)**: Added comprehensive `Build.BOARD` and `Build.HARDWARE` spoofing. Games that check the device board via `__system_property_get` or JNI will now correctly see the profile's SoC platform (e.g., `kalama` / `qcom`) instead of the real device's hardware.
- **Fixed `Build$VERSION` spoofing missing**: The module now properly spoofs `Build$VERSION.SDK_INT` and `Build$VERSION.RELEASE` derived from the profile's fingerprint, preventing mismatch detection.
- **Fixed `Build.DISPLAY` extraction**: The fingerprint parsing logic now extracts the correct display string instead of "release-keys", reducing the risk of anti-tamper detection.
- **Fixed incorrect Game Package Names**: Removed fake and incorrect package variants. BGMI is confirmed to only use `com.pubg.imobile`. PUBG Mobile Lite is now correctly mapped to `com.tencent.iglite`. Free Fire is correctly mapped to `com.dts.freefireth` and the incorrect entry was removed.
