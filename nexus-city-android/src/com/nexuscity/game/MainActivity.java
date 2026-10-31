package com.nexuscity.game;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.InputStream;

/** Full-screen WebView host that serves the bundled game from assets/www over a virtual https origin. */
public class MainActivity extends Activity {
    static final String HOST = "appassets.androidplatform.net";
    private WebView web;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(0x00000080 /* KEEP_SCREEN_ON */ | 0x00000400 /* FULLSCREEN */);
        web = new WebView(this);
        web.setBackgroundColor(0xFF0B0716);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        web.setWebViewClient(new AssetClient(this));
        setContentView(web);
        hideSystemUi();
        web.loadUrl("https://" + HOST + "/index.html");
    }

    void hideSystemUi() {
        // IMMERSIVE_STICKY | LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN | HIDE_NAVIGATION | FULLSCREEN
        web.setSystemUiVisibility(0x1000 | 0x100 | 0x200 | 0x400 | 0x2 | 0x4);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUi();
    }

    @Override
    protected void onPause() {
        if (web != null) web.onPause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (web != null) web.onResume();
    }

    static class AssetClient extends WebViewClient {
        private final Activity activity;

        AssetClient(Activity a) { activity = a; }

        @Override
        public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
            Uri url = request.getUrl();
            if (!HOST.equals(url.getHost())) return null;
            String path = url.getPath();
            if (path == null || path.equals("/")) path = "/index.html";
            try {
                InputStream in = activity.getAssets().open("www" + path);
                return new WebResourceResponse(mime(path), "UTF-8", in);
            } catch (Exception e) {
                return null;
            }
        }

        static String mime(String p) {
            if (p.endsWith(".html")) return "text/html";
            if (p.endsWith(".js")) return "text/javascript";
            if (p.endsWith(".css")) return "text/css";
            if (p.endsWith(".json")) return "application/json";
            if (p.endsWith(".svg")) return "image/svg+xml";
            if (p.endsWith(".png")) return "image/png";
            return "application/octet-stream";
        }
    }
}
