package com.yadavnikhil03.gameunlocker.xposed
object XposedScope {
    const val MODULE_PACKAGE = "com.yadavnikhil03.gameunlocker"
    const val SYSTEM_SERVER_PACKAGE = "android"

    fun isSystemServer(packageName: String): Boolean =
        packageName == SYSTEM_SERVER_PACKAGE
}