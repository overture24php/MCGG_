// ---------------------------------------------------------------------------
// MainActivity — layar izin + tombol mulai/stop overlay menu.
// UI programatik (tanpa res/layout) supaya proyek minim file & tanpa dependency.
// ---------------------------------------------------------------------------
package ph.overture.mcggmodmenu;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {

    private static final int REQ_NOTIF = 10;
    private static final int REQ_WRITE_LEGACY = 11;

    private TextView infoView;
    private Button btnMulai;
    private final Handler handler = new Handler(Looper.getMainLooper());

    private final Runnable refresher = new Runnable() {
        @Override public void run() {
            refreshInfo();
            handler.postDelayed(this, 1500);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(buildUi());

        if (Build.VERSION.SDK_INT >= 33
                && checkSelfPermission("android.permission.POST_NOTIFICATIONS")
                        != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, REQ_NOTIF);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        handler.removeCallbacks(refresher);
        handler.post(refresher);
    }

    @Override
    protected void onPause() {
        super.onPause();
        handler.removeCallbacks(refresher);
    }

    private View buildUi() {
        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (16 * getResources().getDisplayMetrics().density);
        ll.setPadding(p, p, p, p);

        TextView title = new TextView(this);
        title.setText("MCGG Mod Menu");
        title.setTextSize(24);
        ll.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Toggle di menu ini menulis /sdcard/mcggmod.conf —\n"
                + "payload (Zygisk) membaca file itu tiap 2 detik.");
        subtitle.setTextSize(13);
        subtitle.setPadding(0, p / 2, 0, p);
        ll.addView(subtitle);

        btnMulai = addBtn(ll, "Mulai menu mengambang", new View.OnClickListener() {
            @Override public void onClick(View v) {
                if (!Settings.canDrawOverlays(MainActivity.this)) {
                    toast("Izin \"Tampil di atas aplikasi lain\" belum diberikan");
                    openOverlayPermission();
                    return;
                }
                if (!hasStorage()) {
                    toast("Izin akses file belum diberikan");
                    openStoragePermission();
                    return;
                }
                if (Build.VERSION.SDK_INT >= 26) {
                    startForegroundService(new Intent(MainActivity.this, OverlayService.class));
                } else {
                    startService(new Intent(MainActivity.this, OverlayService.class));
                }
                toast("Menu mengambang aktif — buka game");
            }
        });

        addBtn(ll, "Matikan menu mengambang", new View.OnClickListener() {
            @Override public void onClick(View v) {
                stopService(new Intent(MainActivity.this, OverlayService.class));
                toast("Menu dimatikan");
            }
        });

        addBtn(ll, "Izin: tampil di atas aplikasi lain", new View.OnClickListener() {
            @Override public void onClick(View v) { openOverlayPermission(); }
        });

        addBtn(ll, "Izin: akses semua file", new View.OnClickListener() {
            @Override public void onClick(View v) { openStoragePermission(); }
        });

        infoView = new TextView(this);
        infoView.setTextSize(13);
        infoView.setPadding(0, p, 0, 0);
        ll.addView(infoView);

        ScrollView sc = new ScrollView(this);
        sc.addView(ll);
        return sc;
    }

    private Button addBtn(LinearLayout parent, String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setOnClickListener(l);
        parent.addView(b);
        return b;
    }

    private void refreshInfo() {
        boolean ov = Settings.canDrawOverlays(this);
        boolean fs = hasStorage();
        infoView.setText(
                "Overlay diizinkan : " + (ov ? "YA" : "TIDAK") + "\n"
                + "Akses file        : " + (fs ? "YA" : "TIDAK") + "\n"
                + "Menu aktif        : " + (OverlayService.isRunning() ? "YA" : "TIDAK") + "\n\n"
                + "== Status payload ==\n" + ConfStore.status());
        btnMulai.setEnabled(ov && fs);
    }

    private boolean hasStorage() {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        return checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void openOverlayPermission() {
        try {
            startActivity(new Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        } catch (Exception e) {
            toast("Gagal membuka pengaturan overlay: " + e.getMessage());
        }
    }

    private void openStoragePermission() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(
                        Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
                return;
            } catch (Exception ignored) {
                // fallback ke layar umum di bawah
            }
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                return;
            } catch (Exception ignored) {
            }
            toast("Buka: Pengaturan > Aplikasi > MCGG Mod Menu > Akses semua file");
        } else {
            requestPermissions(
                    new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    REQ_WRITE_LEGACY);
        }
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }
}