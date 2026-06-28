# Changelog

## v2.1.0

### Bug Fixes
- **Fixed GPU spoof bypass for Google-profile games** (`GpuHook::isSupported`): Genshin Impact, Honkai Star Rail, and other games mapped to the `PIXEL_9_PRO` profile were silently skipping the GPU hook because the check `manufacturer != "Google"` treated the spoofed profile's manufacturer as if it were the real device. The GPU hook now correctly activates for any active profile.
- **Fixed FC Mobile / FIFA Mobile not working out-of-the-box**: Added `com.ea.gp.fifamobile` to the default routing rules (resolves #13).
- **Fixed WebUI infinite spinner when opened outside KSU/MMRL**: When the WebUI is opened in a plain browser (no root shell injected), the UI now shows a clear error banner — "Shell not available — open via KSU WebUI / MMRL" — instead of hanging on loading spinners forever (resolves #11).

### New Game Profiles Added (default config)
- FC Mobile / FIFA Mobile (`com.ea.gp.fifamobile`) → Samsung S24 Ultra
- Free Fire Global (`com.garena.game.freefire`) → Pixel 9 Pro
- CrossFire Mobile (`com.tencent.tmgp.cf`) → Samsung S24 Ultra
- Honkai: Star Rail Global (`com.HoYoverse.hkrpgoversea`) → Pixel 9 Pro
- Honkai: Star Rail CN (`com.miHoYo.hkrpg`) → Pixel 9 Pro
- Honor of Kings Global (`com.tencent.tmgp.sgame`) → Samsung S24 Ultra
- Honor of Kings Global (`com.levelinfinite.sgame`) → Samsung S24 Ultra

---

## v2.0.2

### Refactoring & Fixes
- Added GLES31 and GLES32 support for broader GPU spoofing coverage
- Tightened `config.json` parsing to ensure strict success semantics
- Cleaned up dead code (empty `onDisable` overrides, unreachable Google GPU branches)
- Fixed `HookManager::initialize` signature mismatch

---

## v2.0.1

### Bug Fixes
- Fixed `service.sh` game detection always failing due to multi-line grep on JSON
- Fixed `uninstall.sh` trying to umount deleted cpuinfo and reset stale properties
- Fixed `customize.sh` unquoted `$count` variable causing errors on some shells
- Fixed WebUI `validPkg` regex allowing empty package segments
- Added `napa` and `napa*` wildcard variants to Qualcomm device detection
- Created proper `changelog.md` for the update channel

---

## v2.0.0

### Architecture
- Complete rewrite using Zygisk V2 API
- Per-app device spoofing via PLT hooks (bytehook)
- JNI `android.os.Build` field injection in `preAppSpecialize`
- Copy-on-write config system with priority-based routing engine
- Zero-daemon WebUI for KSU/APatch/Magisk (via KSU WebUI app)

### Spoofing
- Property hooks cover `ro.product.system.*`, `ro.product.vendor.*`, `ro.product.odm.*` variants
- GPU spoof (Adreno 750) auto-skips non-Qualcomm devices
- Safe `bytehook_init` guarding against double initialization
- Wildcard routing rule for global spoofing with per-app overrides

### Profiles
- Samsung Galaxy S24 Ultra (SM-S928B)
- RedMagic 9 Pro (NX769J)
- Xiaomi 11T Pro (vili)
- Google Pixel 9 Pro (caiman)
- CPU-only spoof mode for competitive titles

### Supported Games
- PUBG Mobile (all regional variants including BGMI)
- Wild Rift, Valorant Mobile, COD Warzone Mobile
- Genshin Impact, Free Fire, Mobile Legends
- Fortnite, Apex Legends Mobile (CPU spoof)
- 30+ titles preconfigured

### Installation
- Standalone Zygisk required (ZygiskNext / ReZygisk / NeoZygisk)
- Built-in Magisk Zygisk is not supported
- Automatic ABI validation during install
- Update channel via `release.json`

