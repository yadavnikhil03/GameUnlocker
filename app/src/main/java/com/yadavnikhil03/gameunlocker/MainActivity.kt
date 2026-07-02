package com.yadavnikhil03.gameunlocker

import android.annotation.SuppressLint
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.webkit.JavascriptInterface
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.app.Activity
import com.topjohnwu.superuser.Shell

class MainActivity : Activity() {

    private lateinit var webView: WebView
    private val handler = Handler(Looper.getMainLooper())

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Initialize libsu global shell
        Shell.setDefaultBuilder(
            Shell.Builder.create()
                .setFlags(Shell.FLAG_REDIRECT_STDERR)
                .setTimeout(10)
        )
        
        // Request root immediately on app launch so the Magisk prompt appears instantly
        Shell.getShell { }

        webView = WebView(this)
        setContentView(webView)

        webView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            cacheMode = WebSettings.LOAD_NO_CACHE
        }

        // Inject the MMRL/KSU polyfill interface so index.html works natively
        webView.addJavascriptInterface(RootShellInterface(), "ksu")

        webView.webViewClient = object : WebViewClient() {
            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
            }
        }

        webView.loadUrl("file:///android_asset/index.html")
    }

    inner class RootShellInterface {
        @JavascriptInterface
        fun exec(command: String, args: String, callbackName: String) {
            // XSS Protection: Strictly validate allowed shell commands
            val isSafe = command.startsWith("cat << 'GU_EOF' > /data/adb/modules/") ||
                         command.startsWith("cat /data/adb/modules/") ||
                         command.startsWith("cmd package list packages") ||
                         (command.startsWith("monkey -p ") && !command.contains(";") && !command.contains("&") && !command.contains("|")) ||
                         command.startsWith("echo \"\$(getprop ") ||
                         command.startsWith("if [ -f /data/adb/modules/") ||
                         command.startsWith("if command -v magisk") ||
                         command.startsWith("if [ -d /data/adb/modules/") ||
                         command.startsWith("grep '^version=' /data/adb/modules/") ||
                         command.startsWith("logcat -d -s GameUnlocker")

            if (!isSafe) {
                handler.post {
                    val js = "$callbackName(1, \"\", \"Security Exception: Command blocked by GameUnlocker XSS filter.\");"
                    webView.evaluateJavascript(js, null)
                }
                return
            }

            Thread {
                var errno = 1
                var stdout = ""
                var stderr = ""
                try {
                    val result = Shell.cmd(command).exec()
                    errno = result.code
                    stdout = result.out.joinToString("\n")
                    stderr = result.err.joinToString("\n")
                } catch (e: Exception) {
                    errno = -1
                    stderr = e.message ?: "Unknown error"
                }
                
                handler.post {
                    // Escape output for JS string injection
                    val safeStdout = stdout.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "")
                    val safeStderr = stderr.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "")
                    
                    val js = "$callbackName($errno, \"$safeStdout\", \"$safeStderr\");"
                    webView.evaluateJavascript(js, null)
                }
            }.start()
        }
    }
}
