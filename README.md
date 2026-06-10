# FPS Unlocker (Zygisk)

![Version](https://img.shields.io/badge/Version-2.0.6--pre.1-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Android-green.svg)
![Magisk](https://img.shields.io/badge/Magisk-Zygisk_Enabled-orange.svg)

**FPS Unlocker** is an advanced Zygisk module designed to unlock higher frame rates in games and enhance overall gaming performance on Android devices, dynamically and safely.

<div align="center">
  <blockquote>
    <h3>✨ Major Update & Pre-Release Testing</h3>
    <p>We are thrilled to be back with a comprehensive overhaul of the GameUnlocker core! We sincerely appreciate the community's patience during our period of inactivity and apologize for the recent compatibility hurdles.</p>
    <p>This latest <b>Pre-Release</b> entirely refactors the Zygisk implementation to ensure flawless compatibility, resolves persistent engine crashes in titles like PUBG/BGMI, and dynamically spoofs the cutting-edge <b>Samsung Galaxy S24 Ultra</b> profile for maximum frame rates.</p>
    <p><i>We warmly invite you to test this build. Your feedback, bug reports, and pull requests are invaluable as we work together to polish this release.</i></p>
  </blockquote>
</div>

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
	- Do **not** use GitHub "Download ZIP" source archives. They may not include compiled `zygisk/*.so` binaries.
2. Make sure **Zygisk** is enabled in your Magisk / KernelSU app.
3. Open **Magisk Manager** / **KernelSU**
4. Tap on **Modules** > **Install from storage**
5. Select the downloaded `.zip` file.
6. **Reboot** your device.

### Troubleshooting: "Zygisk module not loaded due to incompatibility"

- Ensure the installed zip contains `zygisk/arm64-v8a.so` (and `zygisk/armeabi-v7a.so` for 32-bit support).
- If you installed from a source archive, uninstall module, reboot, and install the release asset zip.
- Keep Magisk updated and verify Zygisk is enabled before rebooting.

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

We firmly believe in the power of the open-source community. This module would not be possible without the incredible work of others. We extend our deepest gratitude to:

- **[AlirezaParsi](https://github.com/AlirezaParsi)**: For the architectural design of the [COPG (Call Of PUBG Gaming)](https://github.com/AlirezaParsi/COPG) project. The native C++ implementation of the Zygisk injection pipeline within our module leverages methodologies extensively researched and published within the COPG repository.
- **[topjohnwu](https://github.com/topjohnwu) & the Magisk Development Team**: For the engineering of the Magisk root framework and the Zygisk API, which provide the essential hooking interfaces utilized by this module.
- **[tiann](https://github.com/tiann) & the KernelSU Development Team**: For the continued development of KernelSU, providing an alternative, kernel-level privileged execution environment compatible with our runtime modifications.

---

## Open-Source Compliance

- This repository is open source under the project license.
- If you fork or redistribute this project, keep attribution and license files intact.
- See [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) for third-party attribution and reuse expectations.

---

## License

This project is licensed under the **MIT License** – see the [LICENSE](./LICENSE) file for details.
