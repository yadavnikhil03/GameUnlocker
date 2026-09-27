package com.yadavnikhil03.gameunlocker.xposed
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.callbacks.XC_LoadPackage

class FrameRateHook : IXposedHookLoadPackage {
    companion object {

        private const val TAG = "GameUnlocker/FrameRate"

        private const val FRAME_RATE_COMPATIBILITY_FIXED_SOURCE = 1
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam?) {
        lpparam ?: return
        if (lpparam.packageName == "android") return
        hookSurfaceFrameRate(lpparam.classLoader)
        hookChoreographer(lpparam.classLoader)
        hookAdpfHints(lpparam.classLoader)
    }

    private fun hookSurfaceFrameRate(classLoader: ClassLoader) {
        runCatching {
            XposedHelpers.findAndHookMethod(
                "android.view.Surface",
                classLoader,
                "setFrameRate",
                Float::class.javaPrimitiveType,
                Int::class.javaPrimitiveType,
                Int::class.javaPrimitiveType,
                object : XC_MethodHook() {

                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val requestedFps = param.args[0] as? Float ?: return
                        if (requestedFps > 0f && requestedFps <= 60f) {
                            XposedBridge.log(
                                "$TAG: setFrameRate($requestedFps) → 0f (unlocked)"
                            )
                            param.args[0] = 0f
                            param.args[1] = FRAME_RATE_COMPATIBILITY_FIXED_SOURCE
                        }
                    }
                }
            )
            XposedBridge.log("$TAG: Surface.setFrameRate hook installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: Surface.setFrameRate hook failed: ${e.message}")
        }
        runCatching {
            XposedHelpers.findAndHookMethod(
                "android.view.Surface",
                classLoader,
                "setFrameRate",
                Float::class.javaPrimitiveType,
                Int::class.javaPrimitiveType,
                object : XC_MethodHook() {

                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val requestedFps = param.args[0] as? Float ?: return
                        if (requestedFps > 0f && requestedFps <= 60f) {
                            param.args[0] = 0f
                        }
                    }
                }
            )
        }
    }

    private fun hookChoreographer(classLoader: ClassLoader) {
        runCatching {
            XposedHelpers.findAndHookMethod(
                "android.view.Choreographer",
                classLoader,
                "getFrameIntervalNanos",
                object : XC_MethodHook() {

                    override fun afterHookedMethod(param: MethodHookParam) {
                        val intervalNs = param.result as? Long ?: return
                        if (intervalNs >= 16_666_667L) {
                            XposedBridge.log(
                                "$TAG: Choreographer interval=${intervalNs}ns " +
                                "(${1_000_000_000L / intervalNs}fps)"
                            )
                        }
                    }
                }
            )
        }.onFailure {  }
    }

    private fun hookAdpfHints(classLoader: ClassLoader) {
        runCatching {
            val hintManagerClass = XposedHelpers.findClassIfExists(
                "android.os.PerformanceHintManager",
                classLoader
            ) ?: return@runCatching
            XposedHelpers.findAndHookMethod(
                hintManagerClass,
                "createHintSession",
                IntArray::class.java,           
                Long::class.javaPrimitiveType,  
                object : XC_MethodHook() {

                    override fun beforeHookedMethod(param: MethodHookParam) {
                        val requestedNs = param.args[1] as? Long ?: return
                        if (requestedNs >= 16_666_667L) {
                            XposedBridge.log(
                                "$TAG: ADPF hint ${requestedNs}ns → 8333333ns (120fps)"
                            )
                            param.args[1] = 8_333_333L  
                        }
                    }
                }
            )
            XposedBridge.log("$TAG: ADPF PerformanceHintManager hook installed")
        }.onFailure {  }
    }
}