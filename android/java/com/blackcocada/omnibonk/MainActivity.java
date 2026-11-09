package com.blackcocada.omnibonk;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

/**
 * Thin Java shell around the native game (libomnibonk.so, android_main).
 * Handles immersive fullscreen, display-cutout safe insets and vibration;
 * the native side reads the static fields / calls vibrate() through JNI.
 */
public class MainActivity extends NativeActivity {
    public static volatile int insetL, insetR, insetT, insetB;
    public static volatile String launchArgs = "";
    private Vibrator vibrator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // must be set before super.onCreate(): that call starts the native thread
        Intent it = getIntent();
        if (it != null) {
            String a = it.getStringExtra("args");
            if (a != null) launchArgs = a;
        }
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        getWindow().getDecorView().setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                readCutout(insets);
                return v.onApplyWindowInsets(insets);
            }
        });
        hideSystemUi();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUi();
            readCutout(getWindow().getDecorView().getRootWindowInsets());
        }
    }

    @SuppressWarnings("deprecation")
    private void hideSystemUi() {
        View decor = getWindow().getDecorView();
        // LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN | HIDE_NAVIGATION | FULLSCREEN | IMMERSIVE_STICKY
        decor.setSystemUiVisibility(0x1706);
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
    }

    private void readCutout(WindowInsets insets) {
        if (insets == null) return;
        int l = 0, r = 0, t = 0, b = 0;
        if (Build.VERSION.SDK_INT >= 28) {
            DisplayCutout c = insets.getDisplayCutout();
            if (c != null) {
                l = c.getSafeInsetLeft(); r = c.getSafeInsetRight();
                t = c.getSafeInsetTop(); b = c.getSafeInsetBottom();
            }
        }
        insetL = l; insetR = r; insetT = t; insetB = b;
    }

    /** Called from the native game thread. */
    public void vibrate(int ms) {
        try {
            if (vibrator == null || !vibrator.hasVibrator()) return;
            vibrator.vibrate(VibrationEffect.createOneShot(Math.max(1, ms), VibrationEffect.DEFAULT_AMPLITUDE));
        } catch (Throwable ignored) {
        }
    }
}
