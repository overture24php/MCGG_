# ---------------------------------------------------------------------------
# deploy.ps1 — bantuan deploy/diagnosa mod Android MCGG lewat adb.
# Contoh:
#   .\scripts\deploy.ps1 -Action conf                # push mcggmod.conf
#   .\scripts\deploy.ps1 -Action status              # baca status mod
#   .\scripts\deploy.ps1 -Action logcat              # log MCGGMOD + Zygisk
#   .\scripts\deploy.ps1 -Action zip -ZipIn <folder> # zip folder module Magisk
# ---------------------------------------------------------------------------
param(
    [ValidateSet('conf', 'status', 'logcat', 'zip')]
    [string]$Action = 'status',
    [string]$ConfPath = "$PSScriptRoot\..\mcggmod.conf",
    [string]$ZipIn,
    [string]$ZipOut = "$PSScriptRoot\..\mcgg-module.zip"
)

$ErrorActionPreference = 'Stop'

switch ($Action) {
    'conf' {
        if (-not (Test-Path $ConfPath)) {
            # contoh default kalau belum ada file
            @(
                'preclear=1'
                'autobuy_guin=1'
                'autowin_bypass=1'
                'autowin=0'
                'autostack=0'
                'clear_stack=0'
                'skip_guide=0'
            ) | Set-Content -Path $ConfPath -Encoding ascii
            "conf default dibuat: $ConfPath"
        }
        adb push $ConfPath /sdcard/mcggmod.conf
        'conf terpush — efektif dalam ~2 detik (watcher), tanpa restart game'
    }
    'status' {
        adb shell cat /sdcard/mcggmod_status.txt
    }
    'logcat' {
        adb logcat -s MCGGMOD:V MCGG_Zygisk:V
    }
    'zip' {
        if (-not $ZipIn -or -not (Test-Path $ZipIn)) { throw "folder sumber tidak ada: $ZipIn" }
        if (Test-Path $ZipOut) { Remove-Item $ZipOut -Force }
        Compress-Archive -Path "$ZipIn\*" -DestinationPath $ZipOut
        "module zip: $ZipOut"
    }
}