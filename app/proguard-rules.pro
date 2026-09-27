# =============================================================================
# GameUnlocker ProGuard Rules
# =============================================================================

# WebView JavaScript interface — must be kept or JS bridge silently breaks
-keepclassmembers class com.yadavnikhil03.gameunlocker.MainActivity$RootShellInterface {
    public *;
}

# =============================================================================
# LSPosed / Xposed hooks
# All IXposedHook* implementations MUST be kept with their full class names
# because they are loaded by LSPosed via reflection using the class names
# listed in assets/xposed_init. Obfuscating or removing them = silent failure.
# =============================================================================
-keep class com.yadavnikhil03.gameunlocker.xposed.** { *; }

# Keep Xposed API interfaces (compileOnly — resolved at runtime by LSPosed)
-keep interface de.robv.android.xposed.** { *; }
-keep class de.robv.android.xposed.XposedBridge { *; }
-keep class de.robv.android.xposed.XposedHelpers { *; }
-keep class de.robv.android.xposed.XC_MethodHook { *; }
-keep class de.robv.android.xposed.callbacks.** { *; }

# Keep IXposedHookLoadPackage / IXposedHookZygoteInit
-keep interface de.robv.android.xposed.IXposedHook* { *; }

# libxposed modern API (io.github.libxposed)
-keep class io.github.libxposed.** { *; }
-keep interface io.github.libxposed.** { *; }

# =============================================================================
# libsu — keep Shell API entry points
# =============================================================================
-keep class com.topjohnwu.superuser.** { *; }
