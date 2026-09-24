// ---------------------------------------------------------------------------
// OverlayService — panel mengambang (TYPE_APPLICATION_OVERLAY) berisi 7 toggle.
// 4 toggle persisten: langsung tulis conf saat diubah.
// 3 one-shot: tulis "1", tombol dikunci sampai payload menulis balik "0".
// Polling 1 dtk sinkronkan UI dengan isi file (termasuk reset dari payload).
// Header panel bisa digeser (drag).
// ---------------------------------------------------------------------------
package ph.overture.mcggmodmenu;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.util.HashMap;
import java.util.Map;

public class OverlayService extends Service {

    private static volatile boolean sRunning = false;

    public static boolean isRunning() {
        return sRunning;
    }

    private WindowManager wm;
    private View panelView;
    private TextView header;
    private TextView statusView;
    private final Map<String, Switch> switches = new HashMap<>();
    private final Map<String, Button> oneShotButtons = new HashMap<>();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean suppressUi = false;

    private final Runnable poller = new Runnable() {
        @Override public void run() {
            syncUi();
            handler.postDelayed(this, 1000);
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        sRunning = true;
        startAsForeground();
        wm = (WindowManager) getSystemService(WINDOW_SERVICE);
        buildPanel();
        handler.post(poller);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // panel sudah dibuat di onCreate; start ulang = no-op
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        sRunning = false;
        handler.removeCallbacks(poller);
        if (panelView != null && panelView.getParent() != null) {
            wm.removeView(panelView);
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void buildPanel() {
        int pad = dp(10);

        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackgroundColor(Color.argb(235, 24, 28, 34));

        // header = poin drag
        header = new TextView(this);
        header.setText("  MCGG Mod — geser untuk memindah  ");
        header.setTextSize(14);
        header.setTextColor(Color.WHITE);
        header.setBackgroundColor(Color.argb(255, 40, 90, 160));
        header.setPadding(pad, pad, pad, pad);
        panel.addView(header, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(pad, pad, pad, pad);
        panel.addView(body);

        // 4 toggle persisten
        addSwitch(body, "preclear",       "Shop Pre-Clear");
        addSwitch(body, "autobuy_guin",   "Auto Buy Guinevere");
        addSwitch(body, "autowin_bypass", "Auto Win Bypass");
        addSwitch(body, "autostack",      "Auto Stack 14");

        // 3 one-shot
        addOneShot(body, "autowin",     "Auto Win");
        addOneShot(body, "clear_stack", "Clear Stack");
        addOneShot(body, "skip_guide",  "Skip Guide");

        statusView = new TextView(this);
        statusView.setTextSize(11);
        statusView.setTextColor(Color.argb(255, 170, 200, 170));
        statusView.setPadding(0, pad, 0, 0);
        body.addView(statusView);

        Button close = new Button(this);
        close.setText("Tutup");
        close.setTextSize(12);
        close.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { stopSelf(); }
        });
        body.addView(close);

        panelView = panel;

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                Build.VERSION.SDK_INT >= 26
                        ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                        : WindowManager.LayoutParams.TYPE_PHONE,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.TOP | Gravity.START;
        lp.x = dp(12);
        lp.y = dp(120);

        wm.addView(panelView, lp);
        enableDrag(header, lp);
        syncUi();
    }

    private void addSwitch(LinearLayout parent, final String key, String label) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);

        TextView t = new TextView(this);
        t.setText(label);
        t.setTextColor(Color.WHITE);
        t.setTextSize(13);
        LinearLayout.LayoutParams tl = new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        t.setPadding(0, dp(6), dp(8), dp(6));
        row.addView(t, tl);

        Switch sw = new Switch(this);
        sw.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(CompoundButton btn, boolean checked) {
                if (suppressUi) return;
                ConfStore.set(key, checked ? 1 : 0);
            }
        });
        row.addView(sw);
        parent.addView(row);
        switches.put(key, sw);
    }

    private void addOneShot(LinearLayout parent, final String key, final String label) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);

        TextView t = new TextView(this);
        t.setText(label);
        t.setTextColor(Color.WHITE);
        t.setTextSize(13);
        LinearLayout.LayoutParams tl = new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        t.setPadding(0, dp(6), dp(8), dp(6));
        row.addView(t, tl);

        final Button b = new Button(this);
        b.setText("JALAN");
        b.setTextSize(12);
        b.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                ConfStore.set(key, 1);
                v.setEnabled(false);
                ((Button) v).setText("...");
                Toast.makeText(OverlayService.this,
                        label + " dikirim (sekali jalan)", Toast.LENGTH_SHORT).show();
            }
        });
        row.addView(b);
        parent.addView(row);
        oneShotButtons.put(key, b);
    }

    /** Sinkronkan widget dengan isi file (payload bisa menulis ulang kapan saja). */
    private void syncUi() {
        Map<String, Integer> m = ConfStore.read();
        suppressUi = true;
        for (Map.Entry<String, Switch> e : switches.entrySet()) {
            Switch sw = e.getValue();
            Integer iv = m.get(e.getKey());
            boolean want = iv != null && iv != 0;
            if (sw.isChecked() != want) sw.setChecked(want);
        }
        for (Map.Entry<String, Button> e : oneShotButtons.entrySet()) {
            Integer iv = m.get(e.getKey());
            boolean active = iv != null && iv != 0;
            Button b = e.getValue();
            if (active == b.isEnabled()) {          // file=1 -> kunci; file=0 -> buka
                b.setEnabled(!active);
                b.setText(active ? "..." : "JALAN");
            }
        }
        suppressUi = false;
        if (statusView != null) {
            statusView.setText(ConfStore.status());
        }
    }

    // ------------------------------------------------------------ drag ------

    private void enableDrag(final View handle, final WindowManager.LayoutParams lp) {
        handle.setOnTouchListener(new View.OnTouchListener() {
            private int startX, startY;
            private boolean moved;

            @Override
            public boolean onTouch(View v, MotionEvent ev) {
                switch (ev.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        startX = lp.x - (int) ev.getRawX();
                        startY = lp.y - (int) ev.getRawY();
                        moved = false;
                        return true;
                    case MotionEvent.ACTION_MOVE: {
                        int nx = startX + (int) ev.getRawX();
                        int ny = startY + (int) ev.getRawY();
                        if (Math.abs(nx - lp.x) + Math.abs(ny - lp.y) > dp(3)) moved = true;
                        lp.x = nx;
                        lp.y = ny;
                        wm.updateViewLayout(panelView, lp);
                        return true;
                    }
                    case MotionEvent.ACTION_UP:
                        return moved;   // drag = konsum; tap polos = lepas ke sistem
                }
                return false;
            }
        });
    }

    // ------------------------------------------------------ notif / misc ----

    private void startAsForeground() {
        String channelId = "mcgg_overlay";
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel ch = new NotificationChannel(
                    channelId, "MCGG Mod Menu",
                    NotificationManager.IMPORTANCE_LOW);
            ch.setDescription("Menu mengambang MCGG Mod aktif");
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.createNotificationChannel(ch);
        }

        Intent open = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        Notification.Builder nb = (Build.VERSION.SDK_INT >= 26)
                ? new Notification.Builder(this, channelId)
                : new Notification.Builder(this);
        nb.setSmallIcon(android.R.drawable.ic_menu_manage)
                .setContentTitle("MCGG Mod Menu aktif")
                .setContentText("Ketuk untuk kembali ke panel izin/toggle")
                .setContentIntent(pi)
                .setOngoing(true);

        // API 33+: notif tetap jalan walau izin POST_NOTIFICATIONS belum diberikan
        startForeground(1, nb.build());
    }

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density);
    }
}