package com.yadavnikhil03.gameunlocker.xposed
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.callbacks.XC_LoadPackage
import java.lang.reflect.Field

class ThermalHook : IXposedHookLoadPackage {
    companion object {

        private const val TAG = "GameUnlocker/Thermal"

        private const val THERMAL_STATUS_NONE = 0
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam?) {
        lpparam ?: return
        if (lpparam.packageName == "android") return
        hookPowerManagerThermalStatus(lpparam.classLoader)
        hookThermalManager(lpparam.classLoader)
    }

    private fun hookPowerManagerThermalStatus(classLoader: ClassLoader) {
        runCatching {
            XposedHelpers.findAndHookMethod(
                "android.os.PowerManager",
                classLoader,
                "getCurrentThermalStatus",
                object : XC_MethodHook() {

                    override fun afterHookedMethod(param: MethodHookParam) {
                        val status = param.result as? Int ?: return
                        if (status > THERMAL_STATUS_NONE) {
                            XposedBridge.log(
                                "$TAG: getCurrentThermalStatus() $status → $THERMAL_STATUS_NONE"
                            )
                            param.result = THERMAL_STATUS_NONE
                        }
                    }
                }
            )
            XposedBridge.log("$TAG: PowerManager.getCurrentThermalStatus hook installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: PowerManager thermal hook failed: ${e.message}")
        }
    }

    private fun hookThermalManager(classLoader: ClassLoader) {
        runCatching {
            runCatching {
                XposedHelpers.findAndHookMethod(
                    "android.os.PowerManager",
                    classLoader,
                    "getThermalHeadroom",
                    Int::class.javaPrimitiveType,  
                    object : XC_MethodHook() {

                        override fun afterHookedMethod(param: MethodHookParam) {
                            val headroom = param.result as? Float ?: return
                            if (headroom < 1.0f) {
                                XposedBridge.log(
                                    "$TAG: PowerManager.getThermalHeadroom() $headroom → 1.0"
                                )
                                param.result = 1.0f
                            }
                        }
                    }
                )
            }.onFailure {
            }
            runCatching {
                XposedHelpers.findAndHookMethod(
                    "android.os.ThermalManager",
                    classLoader,
                    "getThermalHeadroom",
                    Int::class.javaPrimitiveType,
                    object : XC_MethodHook() {

                        override fun afterHookedMethod(param: MethodHookParam) {
                            val headroom = param.result as? Float ?: return
                            if (headroom < 1.0f) {
                                XposedBridge.log(
                                    "$TAG: ThermalManager.getThermalHeadroom() → 1.0"
                                )
                                param.result = 1.0f
                            }
                        }
                    }
                )
                XposedBridge.log("$TAG: ThermalManager.getThermalHeadroom hook installed")
            }.onFailure {  }
        }.onFailure { e ->
            XposedBridge.log("$TAG: ThermalManager hook setup failed: ${e.message}")
        }
    }
}