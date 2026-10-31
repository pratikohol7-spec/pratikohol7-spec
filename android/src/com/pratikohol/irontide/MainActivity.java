package com.pratikohol.irontide;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.lang.reflect.Method;

/**
 * Hosts the game (assets/www/index.html) in a full-screen, landscape WebView.
 * Compiled against the API 16 stubs, so newer APIs are called by reflection.
 */
public class MainActivity extends Activity {
    private WebView web;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        int flags = WindowManager.LayoutParams.FLAG_FULLSCREEN | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON;
        getWindow().setFlags(flags, flags);

        web = new WebView(this);
        web.setBackgroundColor(0xff0d0a09);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        call(s, "setMediaPlaybackRequiresUserGesture", boolean.class, false); // API 17+
        web.setWebChromeClient(new WebChromeClient());
        web.setWebViewClient(new WebViewClient());
        setContentView(web);
        hideSystemUi();
        web.loadUrl("file:///android_asset/www/index.html");
    }

    private void hideSystemUi() {
        // IMMERSIVE_STICKY | HIDE_NAVIGATION | FULLSCREEN | LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN
        web.setSystemUiVisibility(0x1000 | 0x2 | 0x4 | 0x100 | 0x200 | 0x400);
    }

    private static void call(Object target, String name, Class<?> type, Object arg) {
        try {
            Method m = target.getClass().getMethod(name, type);
            m.invoke(target, arg);
        } catch (Exception ignored) {
        }
    }

    /** Runs JavaScript (evaluateJavascript is API 19+) and hands the result to the callback. */
    private void js(String code, ValueCallback<String> cb) {
        try {
            Method m = WebView.class.getMethod("evaluateJavascript", String.class, ValueCallback.class);
            m.invoke(web, code, cb);
        } catch (Exception e) {
            web.loadUrl("javascript:" + code);
            if (cb != null) cb.onReceiveValue("true");
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    @Override
    protected void onPause() {
        js("window.__game && window.__game.G.state==='fight' && document.getElementById('resumeBtn') && window.__onBack && window.__onBack()", null);
        web.onPause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        web.onResume();
        hideSystemUi();
    }

    @Override
    public void onBackPressed() {
        // Back pauses / resumes / returns to the title; on the title screen it exits.
        js("window.__onBack ? String(window.__onBack()) : 'false'", new ValueCallback<String>() {
            @Override
            public void onReceiveValue(String v) {
                if (v == null || !v.contains("true")) finish();
            }
        });
    }

    @Override
    protected void onDestroy() {
        if (web != null) web.destroy();
        super.onDestroy();
    }
}
