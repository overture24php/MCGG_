# MCGG Android Mod (arm64)

Mod native untuk `com.mobilechess.gp` v1.2.98.3143 (Unity IL2CPP).
Dibangun sebagai satu `.so` mandiri: frida-gum tertanam (tanpa frida-server),
resolver IL2CPP **by-name** (bukan offset), hook mode attach.

## 7 toggle

| # | Fitur | Toggle di conf | Status |
|---|---|---|---|
| 1 | Shop Pre-Clear | `preclear` | port Scavenger.cs: beli slot murah di awal round, sisakan hero termahal |
| 2 | Auto Buy Guinevere | `autobuy_guin` | port ScheduleAutoBuys/TryExecutePending: beli slot yang sudah ditandai gratis |
| 3 | Auto Win Bypass | `autowin_bypass` | 3 paket: Result/LogRound/Suivive visit(SdpPacker) — default ON |
| 4 | Auto Win | `autowin` | trigger instan, sekali tekan |
| 5 | Auto Stack 14 | `autostack` | port AutoStackTick: hero 1g -> bintang 2 = stack, stop di 14 |
| 6 | Clear Stack | `clear_stack` | reset counter tanpa keluar match |
| 7 | Skip Guide | `skip_guide` | SkipTutorialBattleGuide(true), sekali tekan |

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
skip_guide=0
```

`autowin=1`, `clear_stack=1` dan `skip_guide=1` adalah aksi sekali-pakai —
mod otomatis mengembalikannya ke 0 setelah dijalankan.

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
induk — di proses induk `libil2cpp.so` belum ada. Zygisk loader
(`mcgg-zygisk-module/zygisk/`) berjalan di post-fork child (`postAppSpecialize`),
poll `libil2cpp.so` via `RTLD_NOLOAD` sampai termuat, baru `dlopen`
`files/libmcggmod.so` — payload sendiri menunggu metadata siap (`il2::Wait`).
`zygisk.hpp` bundled di repo (tidak download saat CI build).

**Frame tick.** `UnityEngine.Time.get_deltaTime` (UnityEngine.CoreModule.dll)
di-hook sebagai pengganti `OnUpdate` PC: pre-clear window 200ms, eksekusi
Pending free-buy, dan AutoStackTick dijalankan dari hook ini (thread game).

**Argumen stack.** Hook memuat 16 argumen (x0..x7 + stack) karena
`IShowHandler_CraftHeroAtBattleField` menaruh `newStarLevel` di stack arg
index 11 (layout AAPCS64). 3 craft pertama log nilai mentah `a3..a12` untuk
verifikasi layout dari logcat.

## Resolver v3 (pembacaan tanpa crash)

APK ini dipak `libEncryptor.so`. Setelah dipetakan, pembagiannya:

- `libil2cpp.so` di dalam APK = **stub 384 KB**. Isinya 243 variabel gate
  `m_il2cpp_*_ptr` + thunk `il2cpp_*` 16 byte — **tapi gate-nya tidak pernah
  diisi** packer versi ini (terbukti di HP: `resolve GAGAL: il2cpp_domain_get`
  terus sampai timeout, dan isi `m_il2cpp_domain_get_ptr` tetap null). Stub
  jadi tidak bisa jadi sumber API.
- **il2cpp asli** ada di `/data/data/<pkg>/app_libs/liblogic.so` (162 MB, file
  nyata milik uid app) dan diekspor **langsung** — `il2cpp_domain_get` di
  st_value `0x3c63b84`, total 2.837 dynsym, tanpa gate. Packer mem-map file itu
  dengan custom loader, jadi `dlopen` biasa tidak mengenalnya.

v3 karena itu:
1. **Tidak ada lagi handler `SIGSEGV/SIGBUS` (`sigsetjmp`)**. Versi lama bisa
   `longjmp` keluar dari tengah `malloc` saat packer mencabut halaman modul ->
   thread mod **wedged**: trace berhenti total, status nyangkut
   `waiting_for_il2cpp` selamanya.
2. Semua baca memakai jalur bebas-fault: file -> `pread`, memori ->
   `process_vm_readv` (gagal = `EFAULT`, bukan crash).
3. Urutan sumber symbol: `dlopen(NOLOAD)+dlsym` -> **file `app_libs/liblogic.so`**
   (nilai `st_value` + base dari `/proc/self/maps`) -> gate stub (kalau suatu saat
   diisi) -> symbol langsung di modul memori.
4. Hasil resolve divalidasi: `il2cpp_domain_get` harus berada di region
   executable hasil `/proc/self/maps`; kalau tidak -> ditolak, dicoba lagi.
5. Trace relocate: `/data/data/<pkg>/files/mcggmod_il2_<pid>.txt` dengan prefix
   `[pid t=ms]` (per-proses, tidak tercampur). Ambil dari PC:
   `adb shell "su -c 'cat /data/data/com.mobilechess.gp/files/mcggmod_il2_*.txt'"`

Status file dipisah per-proses: proses game (`:UnityKillsMe`) menulis
`/sdcard/mcggmod_status.txt`; proses shell/extractor (tidak punya il2cpp)
menulis `/sdcard/mcggmod_status_shell.txt` supaya tidak saling menimpa.

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
scripts/deploy.ps1              push conf / baca status / logcat via adb
src/
  main.cpp                      .init_array + JNI_OnLoad, thread utama
  il2cpp.{h,cpp}                resolver by-name + walk parent + image lain
  hook.{h,cpp}                  wrapper frida-gum (16 arg, mode attach)
  config.{h,cpp}                toggle dari /sdcard/mcggmod.conf + watcher
  game.{h,cpp}                  state/hook bersama + frame tick + dispatcher
  log.h                         logcat tag MCGGMOD
  features/
    autowin.cpp                 fitur 3 + 4 + skip guide
    autostack.cpp               fitur 5 + clear stack
    preclear.cpp                fitur 1 (port Scavenger.cs)
    freebuy.cpp                 fitur 2 (port ScheduleAutoBuys)
mcgg-zygisk-module/
  module.prop
  zygisk/main.cpp               loader Zygisk (tunggu libil2cpp -> dlopen payload)
  zygisk/zygisk.hpp             header bundled
```