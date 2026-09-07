#!/system/bin/sh
# service.sh - Zygisk module untuk MCGG Mod Menu
# AMAN: hanya inject ke game, tidak sentuh sistem
# Cara kerja: pasang library ke /data/local/tmp/ saat game dimulai

MODDIR=${0%/*}
LIB_NAME="libmcgg_mod_menu.so"
LIB_SOURCE="$MODDIR/files/$LIB_NAME"
LIB_TARGET="/data/local/tmp/$LIB_NAME"
LOG_FILE="/data/local/tmp/mcgg_inject.log"

log() {
    echo "[$(date '+%H:%M:%S')] $1" >> "$LOG_FILE"
}

log "=== Zygisk module dimulai ==="

# Pastikan library ada
if [ ! -f "$LIB_SOURCE" ]; then
    log "ERROR: Library tidak ditemukan di $LIB_SOURCE"
    exit 1
fi

# Copy library ke /data/local/tmp/ dengan permission yang benar
cp -f "$LIB_SOURCE" "$LIB_TARGET"
chmod 755 "$LIB_TARGET"
log "Library disalin ke $LIB_TARGET"

# Loop: monitor proses game dan inject
# HANYA inject ke child process :UnityKillsMe
INJECTED=""

while true; do
    # Cek apakah game berjalan (child process)
    CHILD_PID=$(pidof -s "com.mobilechess.gp:UnityKillsMe" 2>/dev/null)
    
    if [ -n "$CHILD_PID" ] && [ "$CHILD_PID" != "$INJECTED" ]; then
        log "Game child process ditemukan: PID $CHILD_PID"
        
        # Metode: gunakan magisk policy untuk inject
        # Magisk Delta punya fitur inject via /proc/pid/mem
        
        # Cek apakah proses masih ada
        if [ -d "/proc/$CHILD_PID" ]; then
            log "Proses valid, mencoba inject..."
            
            # Gunakan ptrace untuk inject (via magisk su)
            # Catatan: ini butuh kernel yang support ptrace
            
            # Metode alternatif: pasang sebagai LD_PRELOAD via /proc/pid/environ
            # (tidak selalu bekerja)
            
            # Metode yang paling reliable: hook dlopen via /proc/pid/syscall
            # Tapi ini butuh root dan kernel support
            
            # Untuk sekarang: log saja, inject akan dilakukan oleh native code
            log "Inject akan dilakukan oleh native Zygisk library"
            INJECTED="$CHILD_PID"
        fi
    fi
    
    sleep 5
done
