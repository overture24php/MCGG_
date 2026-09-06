# MCGG Android Mod (arm64)

Mod native untuk `com.mobilechess.gp` v1.2.98.3143 (Unity IL2CPP).
Dibangun sebagai satu `.so` mandiri: frida-gum tertanam (tanpa frida-server),
resolver IL2CPP **by-name** (bukan offset), hook mode attach.

## 6 fitur

| # | Fitur | Toggle di conf | Status |
|---|---|---|---|
| 1 | Shop Pre-Clear | `preclear` | beli slot murah di awal round, sisakan hero termahal |
| 2 | Auto Buy Guinevere | `autobuy_guin` | beli slot yang sudah ditandai gratis |
| 3 | Auto Win Bypass | `autowin_bypass` | bersihkan flag invalid di paket hasil (default ON) |
| 4 | Auto Win | `autowin` | trigger instan, sekali tekan |
| 5 | Auto Stack 14 | `autostack` | hitung hero 1g naik bintang 2, stop di 14 |
| 6 | Clear Stack | `clear_stack` | reset counter tanpa keluar match |

## Build

Repo: `github.com/overture24php/MCGG_` (privat)

Push ke `main` → GitHub Actions build otomatis pakai NDK bawaan runner.
Hasil di tab Actions → artifact **libmcggmod-arm64**:

```
libmcggmod.so          <- untuk System.loadLibrary / dlopen biasa
mcggmod_inject.sh      <- salinan identik, pola libTool (ekstensi .sh, isinya ELF)
```

Trigger manual: tab Actions → workflow `build` → Run workflow.

## Konfigurasi

Mod membaca file toggle tiap 2 detik, jadi bisa diubah **tanpa restart game**:

```
/sdcard/mcggmod.conf
```

Isi (satu `key=0/1` per baris):

```
preclear=1
autobuy_guin=0
autowin_bypass=1
autowin=0
autostack=0
clear_stack=0
```

`autowin=1` dan `clear_stack=1` adalah aksi sekali-pakai — mod otomatis
mengembalikannya ke 0 setelah dijalankan.

## Verifikasi

```
adb logcat -s MCGGMOD
```

Yang harus muncul saat berhasil:

```
=== MCGG mod start (arm64) ===
API il2cpp ter-resolve
Assembly-CSharp.dll ditemukan (N assembly total)
il2cpp SIAP setelah NNN ms
frida-gum siap
hook OK: Cmd_Battle_Result_CS.visit @ 0x...
hook OK: MCLogicHeroShop.Refresh @ 0x...
=== semua hook terpasang ===
```

Kalau muncul `class TIDAK ADA` atau `method TIDAK ADA`, berarti nama berubah
di versi game itu — bukan offset yang salah. Cocokkan ulang dengan dump runtime.

## Catatan teknis penting

**APK ini dipak.** `global-metadata.dat` = 0 byte, `libil2cpp.so` cuma stub
0.37 MB, payload asli disembunyikan di `libResources_z0_0..10` (11 ELF, ~21 MB),
unpacker-nya `libEncryptor.so` + `libflipped.so`.

Konsekuensi: API il2cpp **baru bisa dipakai setelah unpacker selesai**.
`il2::Wait()` melakukan polling `domain_get()` + cari `Assembly-CSharp.dll`
sampai benar-benar ada — bukan menganggapnya siap.

**Namespace wajib eksplisit** (beda dari versi PC):

```
"Battle"     -> MCLogicHeroPool, AntiPluginComp, MCLogicReserveComp,
                MCLogicFightValueComp
"MTTDProto"  -> Cmd_Battle_Result_CS, OperType_Battle_*
""           -> MCLogicHeroShop, MCChessPlayerData, MCLogicBattleData,
                LogicChessManager, MCRelationManager, MCSystemData,
                MCBattleData, BattleReceiveMessageBridge
```

**Game jalan di child process.** Injektor harus menargetkan proses anak, bukan
induk — di proses induk `libil2cpp.so` belum ada.

## Pitfall versi PC yang sudah dihindari di kode ini

| Bug PC | Akibat | Yang dipakai di sini |
|---|---|---|
| pemicu pre-clear `OnStartPrepare` | beli sebelum shop refresh; dipanggil 8x/round | `Refresh(...)` dengan `isAutoRefresh == true` |
| batas beli `_bought + sent` | 1 beli terhitung 2, berhenti di 2 slot | pakai `sent` saja |
| harga dari tabel cost hero | diskon/rule tidak terhitung | `GetItemInfo(slot).m_iPrice` |
| slot berubah isi sebelum op terkirim | salah beli hero mahal | verifikasi ulang harga tepat sebelum kirim |
| stack `newStarLevel >= 2` | bintang 3 ikut dihitung, 1 hero = 2 stack | `== 2` saja |
| stack tanpa filter cost | upgrade gameplay biasa menambah stack | hanya cost 1 |
| stack jalan walau toggle OFF | counter naik sendiri (di PC sampai 17) | cek toggle dulu |
| stack tanpa batas | lewat 14 | berhenti tepat di 14 |
| stack tanpa dedup | hero sama dihitung berkali-kali | dedup per GUID |
| patch `CountBattle.OnBeforeSendBattleResultCS` | stack overflow saat scene battle dimuat | cukup `Cmd_Battle_Result_CS.visit` |

## Yang TIDAK dilakukan, dan alasannya

`m_FreeBuyHeroRate` (Astar.int3, nilai asli 2,1,1) **tidak disentuh**.
Komponen y/z = jumlah hero gratis per pemicu, bukan peluang. Menaikkannya
membuat peer (yang memakai config asli) memotong gold untuk hero ke-2..ke-5 di
sisi mereka. Selisih itu diam saat beli dan meledak saat hero **dijual** →
desync → ketendang. Terbukti di versi PC.

## Risiko yang tetap berlaku

- Akun sama dengan versi PC. Ban menimpa akun, bukan platform.
- `Battle.AntiPluginComp` ada juga di Android: `SendInvaildData`,
  `CheckHurtTooBig`, `CheckBattleTimeTooSmall`, `CheckSkillCdChanged`.
- Auto Win tanpa delay memicu `CheckBattleTimeTooSmall`.
- Injeksi ptrace terlihat lewat `TracerPid` di `/proc/self/status`.

## Struktur

```
CMakeLists.txt
.github/workflows/build.yml     unduh frida-gum devkit + build NDK
src/
  main.cpp                      .init_array + JNI_OnLoad, thread utama
  il2cpp.{h,cpp}                resolver by-name, tunggu packer selesai
  hook.{h,cpp}                  wrapper frida-gum (mode attach)
  config.{h,cpp}                toggle dari /sdcard/mcggmod.conf + watcher
  log.h                         logcat tag MCGGMOD
  features/
    autowin.cpp                 fitur 3 + 4
    autostack.cpp               fitur 5 + 6
    preclear.cpp                fitur 1
    freebuy.cpp                 fitur 2
```
