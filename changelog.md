# GameUnlocker Zygisk Module
#### **Support & Issues**:
- https://github.com/yadavnikhil03/GameUnlocker/issues
---

## v2.1.0-pre.1
### A Complete Reimagining
*   **Native WebUI:** The entire interface was rebuilt from the ground up to support modern MMRL and KernelSU standards. We completely dropped the legacy `action.sh` and CGI-BIN web server. It now loads instantly with zero background daemons.
*   **Native Root JS Integration:** Replaced hacky Java compilation (`AppList.java`) with lightning-fast native `pm list packages` fetching using KSU/MMRL root JavaScript execution (`ksu.exec()`).
*   **C++ Hooking Engine Overhaul:** Fixed a critical architectural flaw that previously caused games to crash (SIGSEGV) when Zygisk unmapped our module. The injection is now 100% memory-safe and persistent for the lifetime of the game.
*   **Dynamic Routing Rules:** The configuration engine was completely rewritten. It now supports a scalable array of `routing_rules` (exact, prefix, wildcard) with an O(1) performance lookup cache for lightning-fast matching.
*   **Plugin-Friendly Hooks:** Decoupled the monolithic C++ code into modular `IHook` plugins (like `GpuHook`), allowing effortless expansion of new spoofing logic in the future.

### Simpler, Safer Spoofing
*   **Clean Zygisk Integration:** Removed all global stack-allocations that could potentially leak. Hook states are now carefully managed on the heap, and PLT hook commitment is correctly handled.
*   **Legacy Cleanup:** Removed all deprecated scripts, significantly reducing the final module size and install complexity.

### Note
*   This is a massive overhaul to stabilize the module for production use. Please report anything unexpected in the GitHub issues.

---

## v2.0.5
### Improvements
*   Initial Zygisk C++ implementation ported over from standard shell scripts.
*   Introduced Samsung S24 Ultra device spoofing profile.
*   Added an experimental WebUI for game management.
