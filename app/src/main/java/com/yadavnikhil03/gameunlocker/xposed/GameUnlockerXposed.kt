package com.yadavnikhil03.gameunlocker.xposed
import android.content.Context
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.IXposedHookZygoteInit
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import android.system.Os
import android.system.OsConstants
import de.robv.android.xposed.callbacks.XC_LoadPackage

class GameUnlockerXposed : IXposedHookLoadPackage, IXposedHookZygoteInit {
    companion object {

        private const val TAG = "GameUnlocker/Xposed"

        private const val GAME_MODE_UNSUPPORTED  = 0

        private const val GAME_MODE_STANDARD     = 1

        private const val GAME_MODE_PERFORMANCE  = 2

        private const val GAME_MODE_BATTERY      = 3

        private const val GAME_MODE_CUSTOM       = 4
    }

    override fun initZygote(startupParam: IXposedHookZygoteInit.StartupParam?) {
        XposedBridge.log("$TAG: initZygote — LSPosed hook module loaded")
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam?) {
        lpparam ?: return
        if (lpparam.packageName != "android") return
        if (!lpparam.isFirstApplication) return
        XposedBridge.log("$TAG: hooking system_server services")
        hookGameManagerService(lpparam.classLoader)
        hookDisplayManagerService(lpparam.classLoader)
        hookPowerManagerService(lpparam.classLoader)
        hookActivityTaskManagerService(lpparam.classLoader)
    }

    private fun hookGameManagerService(classLoader: ClassLoader) {
        runCatching {
            val gameManagerServiceClass = XposedHelpers.findClassIfExists(
                "com.android.server.app.GameManagerService",
                classLoader
            ) ?: return@runCatching
            XposedHelpers.findAndHookMethod(
                gameManagerServiceClass,
                "getGameMode",
                String::class.java,    
                Int::class.javaPrimitiveType, 
                object : XC_MethodHook() {

                    override fun afterHookedMethod(param: MethodHookParam) {
                        val pkg = param.args[0] as? String ?: return
                        val currentMode = param.result as? Int ?: return
                        if (currentMode == GAME_MODE_UNSUPPORTED ||
                            currentMode == GAME_MODE_STANDARD) {
                            param.result = GAME_MODE_PERFORMANCE
                            XposedBridge.log(
                                "$TAG: getGameMode($pkg) overridden: " +
                                "$currentMode → $GAME_MODE_PERFORMANCE"
                            )
                        }
                    }
                }
            )
            val isSupported = gameManagerServiceClass.declaredMethods
                .firstOrNull { it.name.contains("isGameFeatureSupported") }
            if (isSupported != null) {
                XposedBridge.hookMethod(isSupported, object : XC_MethodHook() {

                    override fun afterHookedMethod(param: MethodHookParam) {
                        param.result = true
                    }
                })
            }
            XposedBridge.log("$TAG: GameManagerService hooks installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: GameManagerService hook failed: ${e.message}")
        }
    }

    private fun hookDisplayManagerService(classLoader: ClassLoader) {
        runCatching {
            val directorClass = XposedHelpers.findClassIfExists(
                "com.android.server.display.DisplayModeDirector",
                classLoader
            ) ?: return@runCatching
            directorClass.declaredMethods
                .filter { it.name.contains("GameMode", ignoreCase = true) ||
                          it.name.contains("RefreshRate", ignoreCase = true) }
                .forEach { method ->
                    runCatching {
                        XposedBridge.hookMethod(method, object : XC_MethodHook() {

                            override fun beforeHookedMethod(param: MethodHookParam) {
                                XposedBridge.log(
                                    "$TAG: DisplayModeDirector.${method.name} intercepted"
                                )
                            }
                        })
                    }
                }
            XposedBridge.log("$TAG: DisplayManagerService hooks installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: DisplayManagerService hook failed: ${e.message}")
        }
    }

    private fun hookPowerManagerService(classLoader: ClassLoader) {
        runCatching {
            val powerManagerServiceClass = XposedHelpers.findClassIfExists(
                "com.android.server.power.PowerManagerService",
                classLoader
            ) ?: return@runCatching
            runCatching {
                XposedHelpers.findAndHookMethod(
                    powerManagerServiceClass,
                    "isSustainedPerformanceModeEnabled",
                    object : XC_MethodHook() {

                        override fun afterHookedMethod(param: MethodHookParam) {
                            param.result = true
                        }
                    }
                )
                XposedBridge.log("$TAG: isSustainedPerformanceModeEnabled → true")
            }
            XposedBridge.log("$TAG: PowerManagerService hooks installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: PowerManagerService hook failed: ${e.message}")
        }
    }

    private fun hookActivityTaskManagerService(classLoader: ClassLoader) {
        runCatching {
            val atmsClass = XposedHelpers.findClassIfExists(
                "com.android.server.wm.ActivityTaskManagerService",
                classLoader
            ) ?: return@runCatching
            val methods = atmsClass.declaredMethods.filter {
                it.name == "setFocusedTask" || 
                it.name == "setResumedActivityUncheckLocked" ||
                it.name == "setFocusedActivity"
            }
            for (method in methods) {
                runCatching {
                    XposedBridge.hookMethod(method, object : XC_MethodHook() {

                        override fun afterHookedMethod(param: MethodHookParam) {
                            notifyDaemonEvent()
                        }
                    })
                }
            }
            XposedBridge.log("$TAG: ActivityTaskManagerService hooks installed")
        }.onFailure { e ->
            XposedBridge.log("$TAG: ActivityTaskManagerService hook failed: ${e.message}")
        }
    }

    private fun notifyDaemonEvent() {
        runCatching {
            val path = "/data/local/tmp/gameunlocker/fg_trigger"
            val file = java.io.File(path)
            if (file.exists()) {
                val fd = Os.open(path, OsConstants.O_WRONLY or OsConstants.O_NONBLOCK, 0)
                Os.write(fd, byteArrayOf('1'.code.toByte(), '\n'.code.toByte()), 0, 2)
                Os.close(fd)
            }
        }
    }
}