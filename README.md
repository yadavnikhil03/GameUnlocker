# FPS Unlocker (Zygisk)

![Version](https://img.shields.io/badge/Version-2.0.6--pre.1-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Android-green.svg)
![Magisk](https://img.shields.io/badge/Magisk-Zygisk_Enabled-orange.svg)

FPS Unlocker is a Zygisk module that bypasses game frame rate locks by dynamically spoofing device hardware strings via native C++ hooking.

## Features

- **WebUI Configuration**: Manage settings directly through a local web dashboard (No terminal commands required).
- **Zygisk Injection**: Per-app native hooking using `bytehook` ensuring banking apps remain untouched.
- **Hardware Spoofing Profiles**: Choose between Galaxy S24 Ultra, RedMagic 9 Pro, Xiaomi 11T Pro, or Pixel 9 Pro.
- **CPU Spoof Only**: Unlock frame rates via `/proc/cpuinfo` binding without altering OS branding strings.
- **Smart Thermal Management**: Overrides thermal throttling only while configured games are active.
- **Automated Diagnostics**: Built-in bug reporter that directly captures and extracts module logcats from the root namespace.

## Installation

1. Download the latest `GameUnlocker-Zygisk.zip` from [Releases](../../releases).
2. Install a standalone Zygisk implementation (GameUnlocker is incompatible with built-in Magisk Zygisk).
   - [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)
   - [ReZygisk](https://github.com/PerformanC/ReZygisk)
3. Flash the `.zip` via your root manager (KernelSU, APatch, or Magisk).
4. Reboot device.

## Configuration

The module is configured entirely via the WebUI.

- **KernelSU / APatch:** Tap the module in your app list to launch the WebUI.
- **Magisk:** Install [WebUI-X](https://github.com/MMRLApp/WebUI-X-Portable/releases) or [MMRL](https://github.com/DerGoogler/MMRL) to access the dashboard.

Select your target profile, check the games to inject, and reboot to apply.

## Support

Report bugs via [GitHub Issues](../../issues). Please generate and attach a logcat using the "Report Bug" button in the WebUI.

## Credits

- **[AlirezaParsi](https://github.com/Ali
rezaParsi)**: For the COPG project architecture which forms the foundation of the Zygisk injection pipeline used here.
- **[topjohnwu](https://github.com/topjohnwu)**: Magisk framework and Zygisk API.
- **[tiann](https://github.com/tiann)**: KernelSU development.

## License

MIT License. See [LICENSE](./LICENSE) and [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) for details.
