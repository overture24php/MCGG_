# Cara Pakai MCGG Mod Android (v1.2)

Mod untuk game **Magic Chess: Go Go** `com.mobilechess.gp` v1.2.98.3143 (arm64).
Ada **3 komponen** + 1 APK pendukung. Baca dulu bagian "Mana yang saya butuhkan?"

## Mana yang saya butuhkan?

| Kebutuhan | Pakai ini |
|---|---|
| Cara paling mudah, sekali flash, otomatis tiap buka game | **Magisk module** (wajib HP root + Zygisk) |
| Toggle fitur di layar saat game berjalan | **APK MCGG Mod Menu** (boleh tanpa root, tapi mod payload tetap butuh Zygisk) |
| Injeksi manual per-session (risiko lebih tinggi, anti-detek ptrace) | `libmcggmod.so` + `mcggmod_inject.sh` via jshook/ptrace |
| Menu ImGui lama berbasis JsHook | `libmcgg_mod_menu.so` — **tidak direkomendasikan** (butuh JsHook, sebagian tombol tidak terhubung ke payload) |

## 1. Magisk Module (jalur utama)

**Syarat:** HP sudah root Magisk, **Zygisk aktif** (Magisk → Settings → Zygisk ON), lalu reboot.

1. Salin `mcgg-zygisk-flashable.zip` ke HP.
2. Buka Magisk → **Modules** → **Install from storage** → pilih zip itu.
3. Pastikan pesan instalasi menampilkan `Zygisk: AKTIF` (kalau MATI, aktifkan dulu lalu reboot, flash ulang).
4. Reboot.
5. Buka game. Module menunggu `libil2cpp.so` muncul di proses anak (`:UnityKillsMe`), lalu memuat payload otomatis.

Verifikasi:

```
adb logcat -s MCGG_Zygisk MCGG:V
```

Yang harus muncul: `[+] MCGG zygisk module loaded` → `[+] libil2cpp.so termuat` → `[+] payload libmcggmod.so loaded` → `=== semua hook terpasang ===`.

**Catatan versi zip:**
- Build CI menghasilkan artefak `mcgg-zygisk-magisk` berisi **satu file** `mcgg-zygisk-flashable.zip` — itulah yang diflash (jangan flash zip artefak ganda).
- Modul lama v1.1 (folder tanpa `META-INF`) **tidak bisa** dipasang dari Magisk app; pakai zip v1.2 ke atas.

## 2. APK MCGG Mod Menu (toggle di layar)

APK ini **bukan** payload — ia panel UI mengambang yang menulis `/sdcard/mcggmod.conf`, file yang dibaca payload tiap 2 detik. Jadi payload tetap harus jalan (biasanya lewat Magisk module).

1. Install `MCGGModMenu.apk` (`adb install -r MCGGModMenu.apk` atau buka dari file manager).
2. Buka app → berikan 2 izin:
   - **Tampil di atas aplikasi lain** (overlay mengambang)
   - **Akses semua file** (tulis `/sdcard/mcggmod.conf` di Android 11+)
3. Tekan **Mulai menu mengambang**.
4. Buka game. Panel muncul di layar, bisa digeser (tahan header biru).

Isi panel:

| Toggle | Arti |
|---|---|
| Shop Pre-Clear | beli slot murah awal round, sisakan hero termahal |
| Auto Buy Guinevere | beli slot yang sudah ditandai gratis |
| Auto Win Bypass | bersihkan flag invalid di paket hasil battle (default ON) |
| Auto Stack 14 | kumpulkan hero 1g sampai 14 stack |
| Auto Win (JALAN) | sekali tekan — trigger OnAutoWin |
| Clear Stack (JALAN) | sekali tekan — reset counter stack |
| Skip Guide (JALAN) | sekali tekan — lewati tutorial battle guide |

Tombol **JALAN** otomatis terkunci sampai payload menulis ulang `0` (bukti payload menerima perintah).

## 3. Konfigurasi manual (tanpa APK)

Edit `/sdcard/mcggmod.conf` — format `key=0/1`, berlaku dalam ~2 detik tanpa restart game:

```
preclear=0
autobuy_guin=0
autowin_bypass=1
autowin=0
autostack=0
clear_stack=0
skip_guide=0
```

Dari PC:

```powershell
.\scripts\deploy.ps1 -Action conf     # push conf default
.\scripts\deploy.ps1 -Action status   # baca /sdcard/mcggmod_status.txt
.\scripts\deploy.ps1 -Action logcat   # log MCGGMOD + MCGG_Zygisk
```

`autowin`, `clear_stack`, `skip_guide` = one-shot: tulis `1`, payload mengeksekusi lalu **menulis balik `0` sendiri**. Anda bisa menulis `1` lagi kapan saja untuk mengulang.

## Artefak CI dan isinya

| Artefak | Isi | Untuk apa |
|---|---|---|
| `mcgg-zygisk-magisk` | `mcgg-zygisk-flashable.zip` | flash di Magisk (jalur utama) |
| `mcgg-modmenu-apk` | `MCGGModMenu.apk` | panel toggle mengambang |
| `libmcggmod-arm64` | `libmcggmod.so` + `mcggmod_inject.sh` | injeksi manual (jshook/ptrace); `.sh` = salinan ELF berpola nama libTool |
| `mcgg-modmenu-arm64` | `libmcgg_mod_menu.so` + salinan | menu ImGui era JsHook (legacy) |

## Troubleshooting

| Gejala | Penyebab / solusi |
|---|---|
| Magisk: "not a Magisk module" | zip lama tanpa META-INF → pakai `mcgg-zygisk-flashable.zip` v1.2+ |
| Log `Zygisk: MATI` saat flash | aktifkan Zygisk, reboot, flash ulang |
| `libil2cpp.so tidak muncul dalam 120s` | game belum dibuka / bukan proses anak; buka game sampai loading |
| `class TIDAK ADA` / `method TIDAK ADA` | nama berubah di versi game — cocokkan dengan dump runtime |
| APK tidak bisa tulis conf | izin "Akses semua file" belum diberikan |
| Tombol JALAN tidak terbuka lagi | payload belum menulis balik `0` → cek logcat, pastikan module aktif |
| Re-trigger Auto Win berulang | bug lama v1.1 payload; versi CI baru (edge-trigger) sudah memperbaiki |

## Risiko

- Akun sama dengan versi PC — **ban menimpa akun, bukan platform**.
- `AntiPluginComp` Android: `SendInvaildData`, `CheckBattleTimeTooSmall`, dll — bypass menutup sebagian, bukan jaminan.
- Injeksi ptrace terlihat dari `TracerPid`; jalur Zygisk tidak memakai ptrace.

## Build sendiri

Push ke `main` → GitHub Actions (`github.com/overture24php/MCGG_`) membangun semuanya:

- `mcggmod` — payload frida-gum
- `modmenu` — mod menu ImGui legacy
- `zygisk` — loader + packaging zip flashable
- `apk` — APK mod menu (AGP 7.4.2 + Gradle wrapper, tanpa NDK)