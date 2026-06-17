# GameUnlocker

[![Release](https://img.shields.io/github/v/release/yadavnikhil03/GameUnlocker?label=release)](https://github.com/yadavnikhil03/GameUnlocker/releases/latest)
[![License](https://img.shields.io/github/license/yadavnikhil03/GameUnlocker)](LICENSE)

Zygisk module that unlocks high frame rates (60/90/120 FPS) and graphics settings in Android games by spoofing device identity at the native level.

Works by intercepting system property reads and patching `android.os.Build` fields before the target app initializes, making the game's server-side device whitelist see a flagship device instead of your real hardware.

## How It Works

GameUnlocker operates inside the [Zygisk](https://topjohnwu.github.io/Magisk/guides.html#zygisk) framework (API v4). When Android's Zygote forks a new app process, the module runs two spoofing layers before the app's own code executes:

### 1. Java-level spoofing (`Spoofer.cpp`)

During [`preAppSpecialize`](https://topjohnwu.github.io/Magisk/guides.html#zygisk), the module uses JNI to overwrite static fields on [`android.os.Build`](https://developer.android.com/reference/android/os/Build):

```
Build.MANUFACTURER → "Samsung"
Build.MODEL        → "SM-S928B"
Build.FINGERPRINT  → "samsung/e3qxx/e3q:14/..."
```

This handles any app that reads device info through the standard Java API — which is how most games check your device on launch.

### 2. Native property hooks (`SysPropHook.cpp`)

Some games (especially Unreal Engine titles) bypass `android.os.Build` and call [`__system_property_get()`](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/include/sys/system_properties.h) directly via NDK. The module hooks these libc functions using [bytehook](https://github.com/bytedance/bhook) (PLT hooking):

- `__system_property_get`
- `__system_property_read_callback`
- `__system_property_read`

All `ro.product.*` variants are covered, including the namespaced ones Android 10+ introduced (`ro.product.system.model`, `ro.product.vendor.model`, `ro.product.odm.model`, etc.). See the [Android property system docs](https://source.android.com/docs/core/architecture/configuration/add-system-properties) for background.

### 3. GPU string spoofing (`GpuHook.cpp`)

For games that query OpenGL ES renderer strings via [`GLES20.glGetString()`](https://developer.android.com/reference/android/opengl/GLES20#glGetString(int)), the module hooks the JNI native method to return Adreno 750 identifiers. This hook only activates on Qualcomm devices — it skips Google Tensor and other non-Qualcomm SoCs to avoid returning nonsensical GPU info.

### 4. Routing engine

The config system uses a priority-based routing engine. Each app is matched against routing rules (exact, prefix, suffix, wildcard) sorted by priority. A global wildcard rule at priority 0 provides a default profile, while per-app exact rules at priority 50 override it. Config is parsed once during Zygote's `onLoad` using [nlohmann/json](https://github.com/nlohmann/json) and inherited by forked processes via copy-on-write — no per-app file I/O.

### 5. Performance service (`service.sh`)

A background shell service detects when a configured game is in the foreground and sets Qualcomm GPU performance hints (`vendor.gpu.mode`, `vendor.gfx.low_quality`). Restores defaults when the game exits.

## Requirements

| Requirement | Details |
|---|---|
| Android | 9.0+ (API 27+) |
| Root | [Magisk](https://github.com/topjohnwu/Magisk) ≥ 24.0, [KernelSU](https://github.com/tiann/KernelSU), or [APatch](https://github.com/bmax121/APatch) |
| Zygisk | **Standalone implementation required.** Use [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext), [ReZygisk](https://github.com/PerformanC/ReZygisk), or [NeoZygisk](https://github.com/ponces/NeoZygisk). |
| ABI | `arm64-v8a` or `armeabi-v7a` |

> **Built-in Magisk Zygisk is not supported.** The installer will abort if no standalone Zygisk module is detected. This is intentional — built-in Zygisk has known incompatibilities with bytehook's PLT patching in recent Magisk versions.

## Installation

1. Install a standalone Zygisk implementation (see requirements above).
2. Download `GameUnlocker_v*.zip` from [Releases](https://github.com/yadavnikhil03/GameUnlocker/releases/latest).
3. Flash the ZIP through your root manager (Magisk/KSU/APatch).
4. Reboot.

The installer validates your environment during flash — it checks for a supported root solution, a standalone Zygisk implementation, and verifies the compiled `.so` matches your device ABI.

Existing users: the module registers an [update channel](https://topjohnwu.github.io/Magisk/guides.html#moduleprop) via `release.json`. Your root manager will notify you when a new version is available.

## Configuration

Configuration is managed through a WebUI served by your root manager's built-in webserver.

| Root Manager | How to access |
|---|---|
| KernelSU / APatch | Tap the module entry in the app |
| Magisk | Install [WebUI-X Portable](https://github.com/MMRLApp/WebUI-X-Portable) or [MMRL](https://github.com/DerGoogler/MMRL), then open from there |

From the WebUI you can:
- See all currently configured apps grouped by profile
- Add third-party apps from your installed app list
- Choose which device profile to assign
- Launch a configured game directly
- Remove apps from the config
- Generate a diagnostic report for bug reports

Changes are written to `/data/adb/modules/Game-Unlocker/config.json`. A reboot is required for changes to take effect (since config is loaded at Zygote fork time).

### Manual configuration

The config file is plain JSON. You can edit it directly:

```
adb shell su -c cat /data/adb/modules/Game-Unlocker/config.json
```

See [`common/config.json`](common/config.json) for the full default configuration including all routing rules and profiles.

## Included Profiles

| Profile | Spoofed Device | SoC | Use Case |
|---|---|---|---|
| `SAMSUNG_S24_ULTRA` | SM-S928B (Galaxy S24 Ultra) | Snapdragon 8 Gen 3 | PUBG, BGMI, Wild Rift, Valorant |
| `REDMAGIC_9_PRO` | NX769J (RedMagic 9 Pro) | Snapdragon 8 Gen 3 | COD Mobile, racing games |
| `XIAOMI_11T_PRO` | vili (Xiaomi 11T Pro) | Snapdragon 888 | Tower of Fantasy, Clash of Clans |
| `PIXEL_9_PRO` | caiman (Pixel 9 Pro) | Tensor G4 | Genshin Impact, Free Fire |
| CPU spoof only | — | Snapdragon 8 Gen 2 props | Fortnite, Apex Legends |

## Supported Games

<details>
<summary>30+ preconfigured titles (click to expand)</summary>

**PUBG Mobile variants**
`com.tencent.ig`, `com.pubg.imobile`, `com.pubg.imobile.india`, `com.pubg.imobile.battlegroundsindia`, `com.pubg.imobile.in`, `com.pubg.imobilelite`, `com.pubg.imidas`, `com.pubg.krmobile`, `com.vng.pubgmobile`, `com.rekoo.pubgm`, `com.tencent.tmgp.pubgmhd`

**Riot Games**
`com.riotgames.league.wildrift`, `com.riotgames.league.wildrifttw`, `com.riotgames.league.wildriftvn`, `com.riotgames.valormobile`, `com.tencent.tmgp.codev`

**Other titles**
`com.mobile.legends`, `com.activision.callofduty.warzone`, `com.supercell.brawlstars`, `com.supercell.clashofclans`, `com.supercell.squad`, `com.miHoYo.GenshinImpact`, `com.miHoYo.Yuanshen`, `com.dts.freefireth`, `com.dts.freefiremax`, `com.levelinfinite.hotta.gp`, `com.blizzard.diablo.immortal`, `com.garena.game.df`, `com.pearlabyss.blackdesertm.gl`, `com.nintendo.zaka`

**CPU spoof only** (competitive anti-cheat titles)
`com.epicgames.fortnite`, `com.ea.gp.apexlegendsmobilefps`, `com.miraclegames.farlight84`

</details>

Any app not in the list still gets the default Samsung S24 Ultra profile via the wildcard routing rule.

## Troubleshooting

**Module installs but games show no change**
- Verify your Zygisk implementation is active: check that `/data/adb/modules/zygisksu` (or `rezygisk`/`neozygisk`) exists.
- Make sure you rebooted after installing.
- Check logs: `adb shell logcat -s GameUnlocker` — you should see lines like `GameUnlocker Target Detected: com.tencent.ig`.

**WebUI does not open**
- On Magisk you need [WebUI-X Portable](https://github.com/MMRLApp/WebUI-X-Portable) or [MMRL](https://github.com/DerGoogler/MMRL). KSU and APatch have native WebUI support.

**FPS drops back after a while**
- Another module (thermal limiter, performance tweaker) may be overriding the same system properties. Disable conflicting modules and test.

**Bootloop after install**
- Boot to recovery and delete `/data/adb/modules/Game-Unlocker`.

**Generating a bug report**
- Open the WebUI → tap "Report issue" → tap "Copy report" → paste into a [new issue](https://github.com/yadavnikhil03/GameUnlocker/issues/new).
- Or from terminal: `adb shell logcat -s GameUnlocker -d > gameunlocker_log.txt`

## Building from Source

Requires Android NDK r25c and CMake 3.22+.

```bash
# arm64
mkdir build_arm64 && cd build_arm64
cmake ../cpp \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-27 \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..
mkdir -p zygisk
cp build_arm64/libgameunlocker.so zygisk/arm64-v8a.so

# armeabi-v7a (same steps with -DANDROID_ABI=armeabi-v7a)
```

[bytehook](https://github.com/bytedance/bhook) v1.0.8 is fetched automatically via CMake's `FetchContent`.

To package the flashable ZIP, the [GitHub Actions workflow](.github/workflows/release.yml) handles building both ABIs, signing, and creating a release.

## Project Structure

```
├── cpp/
│   ├── main.cpp              # Zygisk lifecycle (onLoad, preAppSpecialize)
│   ├── Config.cpp            # JSON config parser, routing engine init
│   ├── RoutingEngine.cpp     # Priority-based app→profile matcher
│   ├── SysPropHook.cpp       # PLT hooks for __system_property_*
│   ├── Spoofer.cpp           # JNI android.os.Build field injection
│   ├── GpuHook.cpp           # GLES glGetString hook
│   ├── HookManager.cpp       # Hook lifecycle orchestrator
│   ├── Companion.cpp         # Root companion process (stub)
│   ├── include/nlohmann/     # nlohmann/json header-only lib
│   └── zygisk.hpp            # Zygisk API v4 (upstream, unmodified)
├── common/
│   ├── config.json           # Default device profiles and routing rules
│   └── service.sh            # Background perf-mode service
├── webroot/
│   └── index.html            # WebUI (static HTML/JS, no build step)
├── customize.sh              # Magisk module installer script
├── uninstall.sh              # Cleanup on module removal
├── module.prop               # Module metadata and update channel
└── release.json              # OTA update descriptor
```

## Credits & References

| What | Link |
|---|---|
| Zygisk API | [topjohnwu/Magisk — Zygisk docs](https://topjohnwu.github.io/Magisk/guides.html#zygisk) |
| bytehook (PLT hooking) | [bytedance/bhook](https://github.com/bytedance/bhook) |
| nlohmann/json | [nlohmann/json](https://github.com/nlohmann/json) |
| COPG (architecture reference) | [AlirezaParsi/COPG](https://github.com/AlirezaParsi/COPG) |
| Magisk | [topjohnwu/Magisk](https://github.com/topjohnwu/Magisk) |
| KernelSU | [tiann/KernelSU](https://github.com/tiann/KernelSU) |
| APatch | [bmax121/APatch](https://github.com/bmax121/APatch) |
| ZygiskNext | [Dr-TSNG/ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) |
| ReZygisk | [PerformanC/ReZygisk](https://github.com/PerformanC/ReZygisk) |
| Android property system | [AOSP — System properties](https://source.android.com/docs/core/architecture/configuration/add-system-properties) |
| android.os.Build | [Android SDK reference](https://developer.android.com/reference/android/os/Build) |

## License

MIT — see [LICENSE](LICENSE).

Third-party attributions in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
