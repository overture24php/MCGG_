#!/system/bin/sh
# MCGG Mod Menu — script instalasi Magisk module.
# UI pakai ui_print bawaan Magisk (install_module sudah sediakan).

ui_print "- MCGG Mod Menu (Zygisk loader)"
ui_print "- Payload: files/libmcggmod.so -> di-dlopen ke proses anak game"

# 1. cek Zygisk aktif — module ini TIDAK bekerja tanpa Zygisk
ZYGISK_ON=$(magisk --sqlite "SELECT value FROM settings WHERE key='zygisk'" 2>/dev/null)
if [ "$ZYGISK_ON" = "value=1" ]; then
  ui_print "- Zygisk: AKTIF"
else
  ui_print "! Zygisk: MATI — aktifkan di Magisk > Settings > Zygisk lalu reboot!"
fi

# 2. conf default di /sdcard (payload juga jalan tanpa conf; semua off kecuali bypass)
if [ ! -f /sdcard/mcggmod.conf ]; then
  cat > /sdcard/mcggmod.conf <<'EOF'
# MCGG mod — satu key=0/1 per baris, di-watch payload tiap 2 detik.
# one-shot (autowin / clear_stack / skip_guide) otomatis dikembalikan ke 0.
preclear=0
autobuy_guin=0
autowin_bypass=1
autowin=0
autostack=0
clear_stack=0
skip_guide=0
EOF
  ui_print "- conf default ditulis: /sdcard/mcggmod.conf"
else
  ui_print "- conf lama dipertahankan: /sdcard/mcggmod.conf"
fi

ui_print "- Selesai. Buka game, cek: adb logcat -s MCGGMOD MCGG_Zygisk"