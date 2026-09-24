// ---------------------------------------------------------------------------
// ContentProvider auto-start — dipanggil sistem OTOMATIS tiap kali proses
// game mulai (tanpa patch smali, tanpa izin, tanpa activity).
// Dipakai sebagai komponen utama jalur STANDALONE (APK hasil repack):
//   1. System.loadLibrary("mcggmod") -> payload (hook il2cpp) jalan di proses
//      ini (constructor .so spawn thread sendiri + tunggu il2cpp).
//   2. MCGG.init(...)                 -> pasang lifecycle callback menu UI.
// Dideklarasikan di AndroidManifest oleh standalone/repack.ps1 (via apktool).
// ---------------------------------------------------------------------------
package ph.over.mcgg;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

public class MCGGProvider extends ContentProvider {

    private static final String TAG = "MCGGMENU";

    @Override
    public boolean onCreate() {
        Context ctx = getContext();
        if (ctx == null) return false;

        boolean ok;
        try {
            System.loadLibrary("mcggmod");
            ok = true;
            Log.i(TAG, "payload libmcggmod.so dimuat");
        } catch (Throwable t) {
            ok = false;
            Log.e(TAG, "loadLibrary gagal: " + t);
        }
        MCGG.init(ctx.getApplicationContext(), ok);
        return true;
    }

    // ---- stub wajib ContentProvider (alur query tidak dipakai mod ini) ----
    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) { return null; }

    @Override
    public Uri insert(Uri uri, ContentValues values) { return null; }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) { return 0; }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        return 0;
    }
}