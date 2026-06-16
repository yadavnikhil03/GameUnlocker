# FPS Unlocker (Zygisk)

![Version](https://img.shields.io/badge/Version-2.0.6-blue.svg)

FPS Unlocker bypasses game frame rate locks by dynamically spoofing device hardware strings via native C++ hooking.

## V2 Architecture

The V2 rewrite drops legacy CGI-BIN daemons and bash scripts in favor of a robust, fully native pipeline:
- **Zero-Daemon WebUI**: Configuration is served entirely via the root manager's native webserver (WebUI-X / MMRL / KSU) using static HTML/JS.
- **Copy-On-Write Configuration**: The config file is read once by the Zygote process during `onLoad`. The routing table is stored in static memory and inherited instantly by forked app processes via COW, eliminating file I/O overhead.
- **Native Interception**: Replaced SELinux-blocked mounts with deterministic PLT hooking of `__system_property_get` and `__system_property_read_callback` via `bytehook`.

## Installation

1. Download the latest `GameUnlocker-Zygisk.zip` from Releases.
2. Install a standalone Zygisk implementation. **Built-in Magisk Zygisk is explicitly unsupported and will cause the installation to abort.** Use:
   - ZygiskNext
   - ReZygisk
3. Flash the `.zip` via KernelSU, APatch, or Magisk.
4. Reboot device.

## Configuration

- **KernelSU / APatch:** Tap the module in your app list to launch the WebUI.
- **Magisk:** Install WebUI-X Portable or MMRL to access the dashboard.
Select your target profile, check the games to inject, and reboot to apply.

## Troubleshooting

- **WebUI Does Not Open:** Ensure you are using KSU/APatch or have installed WebUI-X/MMRL for Magisk.
- **Game Unlocks Randomly Drop:** Check if another module (e.g., thermal limiters) is overriding system properties.
- **Module Installs But No Effect:** Verify that your standalone Zygisk implementation is active and that the game is checked in the WebUI.
- **Capturing Logs:** Use the native "Report a Bug" button in the WebUI to instantly generate a diagnostic bundle, or run `logcat -s GameUnlocker`.

## Credits

- AlirezaParsi: COPG project architecture.
- topjohnwu: Magisk framework.
- tiann: KernelSU development.

## License

MIT License. See LICENSE and THIRD_PARTY_NOTICES.md for details.
