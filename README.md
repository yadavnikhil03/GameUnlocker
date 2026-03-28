# FPS Unlocker (Zygisk)

![Version](https://img.shields.io/badge/Version-2.0.0-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Android-green.svg)
![Magisk](https://img.shields.io/badge/Magisk-Zygisk_Enabled-orange.svg)

**FPS Unlocker** is an advanced Zygisk module designed to unlock higher frame rates in games and enhance overall gaming performance on Android devices, dynamically and safely.

---

## Features

- Unlocks **60/90/120 FPS** in supported games using advanced `/proc/cpuinfo` binding.
- **Dynamic Device Spoofing** via Zygisk C++ per-app hooking (Keeps banking apps safe!)
- **Smart Thermal Management**: Only overrides thermal throttling while you are actively playing a game, restoring normal parameters automatically when closed.
- Reduced **input lag** and forced performance rendering.
- JSON based `config.json` for easy management of spoofed games.

---

## Installation

1. Download the latest `GameUnlocker-Zygisk.zip` from the [Releases](../../releases) page.
2. Make sure **Zygisk** is enabled in your Magisk / KernelSU app.
3. Open **Magisk Manager** / **KernelSU**
4. Tap on **Modules** > **Install from storage**
5. Select the downloaded `.zip` file.
6. **Reboot** your device.

---

## Supported Games

- PUBG Mobile / BGMI
- Call of Duty: Mobile
- Asphalt 9
- Genshin Impact
- Mobile Legends
- Free Fire
- *...and many more!*

---

## Contact & Support

**Developer:** [@yadavnikhil03](https://github.com/yadavnikhil03)
**Issues:** Please report bugs via [GitHub Issues](../../issues)

---

## Credits & Acknowledgments

- Huge thanks to [AlirezaParsi](https://github.com/AlirezaParsi) and his [COPG (Call Of PUBG Gaming)](https://github.com/AlirezaParsi/COPG) project. The core Zygisk C++ spoofing engine and approach in this module were heavily inspired by and adapted from the COPG repository.

---

## License

This project is licensed under the **MIT License** – see the [LICENSE](./LICENSE) file for details.
