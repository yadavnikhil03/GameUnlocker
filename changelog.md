# Changelog

## v2.1.0 (2026-06-11)

### Added
- Multi-device profile spoofing (Samsung S24 Ultra, RedMagic 9 Pro, Xiaomi 11T Pro, Pixel 9 Pro)
- 50+ new game packages out of the box
- Dynamic per-app device spoofing in Zygisk module
- CPU spoof unmount support for banking app safety
- Remote config update via Action button
- Root solution detection (Magisk/KernelSU/APatch)
- Zygisk implementation detection (Zygisk Next/ReZygisk/NeoZygisk)
- Game removal from WebUI
- Multi-profile game list display in WebUI
- Clean uninstall script
- SELinux context fix on boot

### Fixed
- CPU spoof persisting after game exit (banking app risk)
- SELinux denial on config.json read
- No cleanup on module uninstall

### Changed
- Config format now supports multiple device profiles per game
- Action button now offers config update before launching WebUI

---

## v2.0.6-pre.1

### Added
- Initial Zygisk C++ implementation
- Samsung S24 Ultra device spoofing
- WebUI for game management
- Bug report with auto-attached logs
- Smart thermal management
- CPU info spoofing via mount bind

### Supported Games
- PUBG Mobile / BGMI variants
- Wild Rift
- Valorant Mobile
- Fortnite (CPU spoof)
- Apex Legends Mobile (CPU spoof)
