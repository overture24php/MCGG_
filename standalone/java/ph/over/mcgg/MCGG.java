// ---------------------------------------------------------------------------
// Menu in-process jalur STANDALONE — menempel di jendela activity game via
// addContentView (tanpa izin SYSTEM_ALERT_WINDOW; area di luar panel tetap
// meneruskan sentuhan ke game).
// Kontrol memanggil JNI bridge payload (src/bridge.cpp) langsung — tanpa
// roundtrip file /sdcard/mcggmod.conf, respons seketika.
// Dipanggil dari MCGGProvider (ContentProvider auto-start di manifest repack).
// ---------------------------------------------------------------------------
package ph.over.mcgg;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

public final class MCGG implements Application.ActivityLifecycleCallbacks {

    // urutan persistent harus sama dengan cfg::Toggles / bridge KeyBit()
    private static final String[] PERSIST = {
            "preclear", "autobuy_guin", "autowin_bypass", "autostack"
    };
    private static final String[] PERSIST_LABEL = {
            "Shop Pre-Clear", "Auto Buy Guinevere",
            "Auto Win Bypass (default)", "Auto Stack 14"
    };
    // {key, label tombol one-shot}
    private static final String[][] ONESHOT = {
            {"autowin",     "Auto Win"},
            {"skip_guide",  "Skip Guide"},
            {"clear_stack", "Reset Stack"},
    };

    // ---- natives: diresolusi dari libmcggmod.so (src/bridge.cpp) ----
    public static native void setPersist(String key, boolean on);
    public static native boolean getPersist(String key);
    public static native void trigger(String key);

    private static boolean sInited;
    private static boolean sNativeOk;
    private static Activity sHost;
    private static LinearLayout sBody;     // konten yang bisa disembunyikan
    private static TextView sStatus;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    private MCGG() {}

    /** Dipanggil MCGGProvider.onCreate (main thread). */
    static void init(Context app, boolean nativeOk) {
        sNativeOk = nativeOk;
        if (sInited || !(app instanceof Application)) return;
        sInited = true;
        ((Application) app).registerActivityLifecycleCallbacks(new MCGG());
    }

    // ---- izin storage (targetSdk 35 + Android 10 = scoped storage + izin runtime) ----
    // Tanpa izin ini payload tidak bisa menulis /sdcard/mcggmod_status.txt dan
    // menu tidak bisa membacanya ("menunggu payload" selamanya). Izin berlaku
    // per-UID jadi cukup diminta sekali dari proses mana pun.
    private static boolean sPermAsked;

    private static void askStoragePerm(Activity a) {
        if (sPermAsked || a == null) return;
        sPermAsked = true;
        try {
            // Refleksi: android.jar stub = API16, sedangkan checkSelfPermission/
            // requestPermissions baru ada di API23 (perangkat target API29+ = ada).
            int granted = (Integer) a.getClass()
                    .getMethod("checkSelfPermission", String.class)
                    .invoke(a, "android.permission.WRITE_EXTERNAL_STORAGE");
            if (granted == 0) {
                Log.i("MCGGMENU", "izin storage sudah ada");
                return;
            }
            a.getClass()
                    .getMethod("requestPermissions", String[].class, int.class)
                    .invoke(a, new String[]{
                            "android.permission.WRITE_EXTERNAL_STORAGE",
                            "android.permission.READ_EXTERNAL_STORAGE"}, 7701);
            Log.i("MCGGMENU", "dialog izin storage diminta");
        } catch (Throwable t) {
            Log.e("MCGGMENU", "minta izin storage gagal: " + t);
        }
    }


    // ---- ActivityLifecycleCallbacks: pasang/repas panel tiap resume ----
    @Override
    public void onActivityResumed(Activity activity) { attach(activity); }

    @Override
    public void onActivityDestroyed(Activity activity) {
        if (sHost == activity) {
            sHost = null;
            sStatus = null;   // hentikan poll status berikutnya
        }
    }

    @Override public void onActivityCreated(Activity a, Bundle b) {}
    @Override public void onActivityStarted(Activity a) {}
    @Override public void onActivityPaused(Activity a) {}
    @Override public void onActivityStopped(Activity a) {}
    @Override public void onActivitySaveInstanceState(Activity a, Bundle b) {}

        // ---- pembuatan panel ----
    private static View sPanel;

    private static int dp(Context c, int d) {
        return Math.round(d * c.getResources().getDisplayMetrics().density);
    }

    // pembungkus aman: kalau .so tidak punya natives (payload lama), jangan crash
    private static boolean safeGet(String key, boolean def) {
        if (!sNativeOk) return def;
        try {
            return getPersist(key);
        } catch (Throwable t) {
            sNativeOk = false;
            return def;
        }
    }

    private static void safeSet(String key, boolean on) {
        if (!sNativeOk) return;
        try {
            setPersist(key, on);
        } catch (Throwable t) {
            sNativeOk = false;
        }
    }

    private static void safeTrigger(String key) {
        if (!sNativeOk) return;
        try {
            trigger(key);
        } catch (Throwable t) {
            sNativeOk = false;
        }
    }

    private static void attach(final Activity act) {
        askStoragePerm(act);
        if (sPanel != null && sHost == act && sPanel.getParent() != null) return;
        detachPanel();
        sHost = act;
        Log.i("MCGGMENU", "attach panel -> " + act.getClass().getSimpleName()
                + " (proc " + android.os.Process.myPid() + ")");
        int pad = dp(act, 10);

        final LinearLayout panel = new LinearLayout(act);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(pad, pad, pad, pad);
        panel.setBackgroundColor(0xDD101820);

        // header = judul + tombol sembunyi + area drag
        final LinearLayout head = new LinearLayout(act);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.setPadding(0, 0, 0, pad / 2);
        TextView title = new TextView(act);
        title.setText("MCGG MOD");
        title.setTextColor(0xFF40C4FF);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(16f);
        final TextView hide = new TextView(act);
        hide.setText(" \u2013 ");
        hide.setTextColor(Color.WHITE);
        hide.setTextSize(20f);
        head.addView(title, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        head.addView(hide);
        panel.addView(head);

        sBody = new LinearLayout(act);
        sBody.setOrientation(LinearLayout.VERTICAL);
        panel.addView(sBody);

        if (!sNativeOk) {
            TextView warn = new TextView(act);
            warn.setText("payload .so tidak termuat — toggle nonaktif");
            warn.setTextColor(0xFFFF5252);
            warn.setTextSize(12f);
            sBody.addView(warn);
        }

        // 4 switch persistent — status awal dari memori payload (bukan file)
        for (int i = 0; i < PERSIST.length; i++) {
            final String key = PERSIST[i];
            final Switch sw = new Switch(act);
            sw.setText(PERSIST_LABEL[i]);
            sw.setTextColor(Color.WHITE);
            sw.setTextSize(13f);
            sw.setChecked(safeGet(key, "autowin_bypass".equals(key)));
            sw.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    safeSet(key, sw.isChecked());
                }
            });
            sBody.addView(sw);
        }

        // baris 3 tombol one-shot
        LinearLayout row = new LinearLayout(act);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(act, 6), 0, 0);
        for (int i = 0; i < ONESHOT.length; i++) {
            final String key = ONESHOT[i][0];
            final Button b = new Button(act);
            b.setText(ONESHOT[i][1]);
            b.setTextSize(12f);
            b.setAllCaps(false);
            LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(
                    0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            blp.leftMargin = dp(act, 4);
            b.setLayoutParams(blp);
            b.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    safeTrigger(key);
                    v.setEnabled(false);   // kunci ~1.5s (payload konsumsi <=500ms)
                    MAIN.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            b.setEnabled(true);
                        }
                    }, 1500);
                }
            });
            row.addView(b);
        }
        sBody.addView(row);

        sStatus = new TextView(act);
        sStatus.setTextColor(0xFFAAAAAA);
        sStatus.setTextSize(11f);
        sStatus.setText(readStatus());
        sBody.addView(sStatus);

        // tempel ke jendela activity (area di luar panel tetap menerus sentuhan)
        act.addContentView(panel, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        panel.setX(dp(act, 12));
        panel.setY(dp(act, 96));
        sPanel = panel;

        // drag via header
        head.setOnTouchListener(new View.OnTouchListener() {
            private float ox, oy;

            @Override
            public boolean onTouch(View v, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        ox = e.getRawX() - panel.getX();
                        oy = e.getRawY() - panel.getY();
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        panel.setX(e.getRawX() - ox);
                        panel.setY(e.getRawY() - oy);
                        return true;
                    default:
                        return false;
                }
            }
        });

        // tombol sembunyi / tampilkan
        hide.setOnClickListener(new View.OnClickListener() {
            private boolean shown = true;

            @Override
            public void onClick(View v) {
                shown = !shown;
                sBody.setVisibility(shown ? View.VISIBLE : View.GONE);
                hide.setText(shown ? " \u2013 " : " + ");
            }
        });

        sGen++;
        startPoll(sGen);
    }

    private static void detachPanel() {
        if (sPanel != null) {
            ViewParent p = sPanel.getParent();
            if (p instanceof ViewGroup) ((ViewGroup) p).removeView(sPanel);
            sPanel = null;
        }
        sBody = null;
        sStatus = null;
        sGen++;   // batalkan poll lama
    }

    // poll status payload tiap 1 detik; gen cocokkan supaya tidak dobel
    private static int sGen;

    private static void startPoll(final int gen) {
        MAIN.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (gen != sGen) return;
                // Re-attach defensif: activity game Unity (proses :UnityKillsMe)
                // bisa mengganti content view / menambah view penuh SETELAH
                // onResume, sehingga panel ikut terhapus / tertutup. Pasang
                // ulang kalau hilang; kalau masih ada, angkat ke paling atas.
                try {
                    Activity a = sHost;
                    if (a != null && !a.isFinishing()) {
                        if (sPanel == null || sPanel.getParent() == null) {
                            Log.i("MCGGMENU", "panel hilang, pasang ulang");
                            attach(a);
                        } else {
                            sPanel.bringToFront();
                        }
                    }
                } catch (Throwable ignored) {
                }
                if (sStatus == null) return;
                sStatus.setText(readStatus());
                startPoll(gen);
            }
        }, 1000);
    }

    private static String readStatus() {
        BufferedReader r = null;
        try {
            r = new BufferedReader(new FileReader("/sdcard/mcggmod_status.txt"));
            String a = r.readLine();
            String b = r.readLine();
            if (a == null) return "(file kosong)";
            return b == null ? a : a + " | " + b;
        } catch (Throwable t) {
            return "(menunggu payload — buka game tunggu loading)";
        } finally {
            if (r != null) {
                try {
                    r.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }
}