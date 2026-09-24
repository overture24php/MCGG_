// ---------------------------------------------------------------------------
// ConfStore — baca/tulis /sdcard/mcggmod.conf (format sama dengan payload).
// Tulis atomik (tmp + rename) supaya watcher payload tidak pernah baca file
// setengah jadi. Payload juga menulis file ini (reset one-shot ke 0) — karena
// pembaca mulai dari default lalu override per-key, file yang terpotong sekalipun
// tidak merusak state.
// ---------------------------------------------------------------------------
package ph.overture.mcggmodmenu;

import android.os.Environment;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.LinkedHashMap;
import java.util.Map;

public final class ConfStore {

    // urutan baris di file = urutan ini (payload tidak peduli urutan, tapi rapi)
    public static final String[] KEYS = {
            "preclear", "autobuy_guin", "autowin_bypass",
            "autowin", "autostack", "clear_stack", "skip_guide"
    };

    // one-shot: dikirim "1", payload mengeksekusi lalu menulis balik "0"
    public static final String[] ONE_SHOT = {
            "autowin", "clear_stack", "skip_guide"
    };

    private static final Charset UTF8 = Charset.forName("UTF-8");

    private ConfStore() {}

    public static File file() {
        return new File(Environment.getExternalStorageDirectory(), "mcggmod.conf");
    }

    public static File statusFile() {
        return new File(Environment.getExternalStorageDirectory(), "mcggmod_status.txt");
    }

    public static boolean isOneShot(String key) {
        for (String k : ONE_SHOT) if (k.equals(key)) return true;
        return false;
    }

    public static Map<String, Integer> read() {
        Map<String, Integer> m = defaults();
        File f = file();
        if (!f.canRead()) return m;
        try (BufferedReader r = new BufferedReader(new FileReader(f))) {
            String line;
            while ((line = r.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                int eq = line.indexOf('=');
                if (eq <= 0) continue;
                String key = line.substring(0, eq).trim();
                if (!m.containsKey(key)) continue;
                try {
                    int v = Integer.parseInt(line.substring(eq + 1).trim());
                    m.put(key, v != 0 ? 1 : 0);
                } catch (NumberFormatException ignored) {
                }
            }
        } catch (IOException ignored) {
        }
        return m;
    }

    /** read-modify-write satu key. synchronized: dua writer (activity+service). */
    public static synchronized void set(String key, int value) {
        Map<String, Integer> m = read();
        m.put(key, value != 0 ? 1 : 0);
        writeAll(m);
    }

    public static synchronized void writeAll(Map<String, Integer> m) {
        File target = file();
        File tmp = new File(target.getParentFile(), target.getName() + ".tmp");
        StringBuilder sb = new StringBuilder();
        for (String k : KEYS) {
            Integer v = m.get(k);
            int iv = v != null ? v : 0;
            sb.append(k).append('=').append(iv != 0 ? 1 : 0).append('\n');
        }
        try (FileOutputStream out = new FileOutputStream(tmp)) {
            out.write(sb.toString().getBytes(UTF8));
            out.getFD().sync();
        } catch (IOException e) {
            return;
        }
        // rename atomik di filesystem POSIX/FUSE
        if (target.exists() && !target.delete()) return;
        //noinspection ResultOfMethodCallIgnored
        tmp.renameTo(target);
    }

    private static Map<String, Integer> defaults() {
        Map<String, Integer> m = new LinkedHashMap<>();
        for (String k : KEYS) m.put(k, 0);
        m.put("autowin_bypass", 1);   // default ON, sama dengan payload
        return m;
    }

    /** Potongan isi /sdcard/mcggmod_status.txt untuk ditampilkan di UI. */
    public static String status() {
        File f = statusFile();
        if (!f.canRead()) return "(belum ada — game + mod belum berjalan)";
        StringBuilder sb = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new FileReader(f))) {
            String line;
            int n = 0;
            while ((line = r.readLine()) != null && n < 12) {
                if (n > 0) sb.append('\n');
                sb.append(line);
                n++;
            }
        } catch (IOException e) {
            return "(gagal baca: " + e.getMessage() + ")";
        }
        return sb.length() > 0 ? sb.toString() : "(file kosong)";
    }

    /** Coba baca file konfig (uji cepat apakah akses storage berfungsi). */
    public static boolean storageWorks() {
        try {
            return file().exists() || file().createNewFile();
        } catch (IOException e) {
            return false;
        }
    }

    // pembaca byte kecil untuk deteksi perubahan (opsional, tidak dipakai UI)
    @SuppressWarnings("unused")
    private static byte[] readBytes(File f) {
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] b = new byte[(int) f.length()];
            int off = 0;
            while (off < b.length) {
                int r = in.read(b, off, b.length - off);
                if (r < 0) break;
                off += r;
            }
            return b;
        } catch (IOException e) {
            return new byte[0];
        }
    }
}