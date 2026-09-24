#requires -Version 5.1
<#
.SYNOPSIS
  Repack APK Magic Chess Go Go jadi versi STANDALONE:
  - sisip libmcggmod.so (arm64-v8a) + menu.dex (menu Java in-game)
  - patch AndroidManifest: auto-start via <provider> (tanpa permission tambahan)
  - sign ulang dengan key lokal yang digenerate otomatis

  Tidak butuh root/Magisk/Zygisk. Cukup uninstall APK lama, install hasil repack.

.EXAMPLE
  pwsh -File .\repack.ps1
  pwsh -File .\repack.ps1 -Apk "D:\game.apk" -Payload "D:\libmcggmod.so"
#>
[CmdletBinding()]
param(
    [string]$Apk = "D:\MyGames\MCGG\MC_ANDROID\com.mobilechess.gp v1.2.98.3143_antisplit.apk",
    [string]$Payload = "",
    [string]$Out = "",
    [string]$WorkDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ToolsDir  = Join-Path $ScriptDir 'tools'
if (-not $Out)     { $Out     = Join-Path $ScriptDir 'out\mcgg-standalone.apk' }
if (-not $WorkDir) { $WorkDir = Join-Path $ScriptDir 'work' }

$ApkToolJar  = Join-Path $ToolsDir 'apktool.jar'
$AndroidJar  = Join-Path $ToolsDir 'android.jar'
$R8Jar       = Join-Path $ToolsDir 'r8.jar'
$BtDir       = Join-Path $ToolsDir 'bt\android-14'
$KeyStore    = Join-Path $ToolsDir 'mcgg-repack.keystore'
$KeyAlias    = 'mcgg'
$KeyPass     = 'mcggrepack123'

function Write-Step([string]$msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }
function Fail([string]$msg) { Write-Host "GAGAL: $msg" -ForegroundColor Red; exit 1 }

# PS 5.1 + EAP=Stop: stderr native tool = terminating error. Bungkus supaya aman.
function Invoke-Native([string]$File, [string[]]$ArgList) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $File @ArgList 2>&1 | ForEach-Object { "$_" } | Write-Host
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    return $code
}

function Get-File([string]$Path, [string]$Url, [string]$Desc) {
    if (Test-Path $Path) { return }
    Write-Host "  unduh $Desc ..."
    $wc = New-Object System.Net.WebClient
    $wc.Headers.Add('User-Agent', 'Mozilla/5.0')
    try { $wc.DownloadFile($Url, $Path) } catch { Fail "unduh $Desc gagal: $($_.Exception.Message)" }
    if ((Get-Item $Path).Length -lt 100000) { Fail "unduh $Desc terlalu kecil (korup?)" }
}

# ---------------------------------------------------------------- tools
Write-Step "Cek & bootstrap tools"
New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

Get-File $ApkToolJar 'https://github.com/iBotPeaches/Apktool/releases/download/v2.11.1/apktool_2.11.1.jar' 'apktool 2.11.1'
Get-File $AndroidJar 'https://repo1.maven.org/maven2/com/google/android/android/4.1.1.4/android-4.1.1.4.jar' 'android.jar (API16 stub)'
Get-File $R8Jar 'https://repo1.maven.org/maven2/com/android/tools/r8/8.3.37/r8-8.3.37.jar' 'r8 8.3.37'

if (-not (Test-Path (Join-Path $BtDir 'zipalign.exe'))) {
    $btZip = Join-Path $ToolsDir 'bt.zip'
    Get-File $btZip 'https://dl.google.com/android/repository/build-tools_r34-windows.zip' 'build-tools r34'
    Write-Host "  ekstrak build-tools ..."
    $btTmp = Join-Path $ToolsDir 'bt'
    Expand-Archive -Path $btZip -DestinationPath $btTmp -Force
    $found = Get-ChildItem $btTmp -Recurse -Filter zipalign.exe | Select-Object -First 1
    if (-not $found) { Fail "zipalign.exe tidak ketemu di build-tools" }
    $script:BtDir = $found.DirectoryName
}

$ZipAlign  = Join-Path $BtDir 'zipalign.exe'
$ApkSigner = Join-Path $BtDir 'apksigner.bat'
foreach ($p in $ZipAlign, $ApkSigner) { if (-not (Test-Path $p)) { Fail "$p tidak ada" } }

$headBytes = [System.IO.File]::ReadAllBytes($ApkToolJar)[0..1]
if ($headBytes[0] -ne 0x50 -or $headBytes[1] -ne 0x4B) { Fail "apktool.jar korup (bukan zip). Hapus $ApkToolJar lalu ulangi." }

# ---------------------------------------------------------------- input
Write-Step "Cek input"
if (-not (Test-Path $Apk)) { Fail "APK tidak ada: $Apk" }

if (-not $Payload) {
    $candidates = @(
        (Join-Path $ScriptDir 'libmcggmod.so'),
        (Join-Path $ScriptDir '..\libmcggmod.so'),
        (Join-Path $ScriptDir '..\build\arm64-v8a\libmcggmod.so')
    )
    foreach ($c in $candidates) { if (Test-Path $c) { $Payload = (Resolve-Path $c).Path; break } }
}
if (-not $Payload -or -not (Test-Path $Payload)) {
    Fail "libmcggmod.so tidak ketemu. Build dulu (CI artifact) lalu taruh di samping repack.ps1, atau: -Payload <path>"
}
Write-Host "  APK     = $Apk"
Write-Host "  payload = $Payload ($([math]::Round((Get-Item $Payload).Length/1MB,1)) MB)"


# ---------------------------------------------------------------- menu dex
Write-Step "Kompilasi menu Java -> classes.dex"
$ClsDir = Join-Path $ScriptDir 'classes'
$DexDir = Join-Path $ScriptDir 'dex'
Remove-Item $ClsDir, $DexDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $ClsDir, $DexDir | Out-Null

$javaSrc = Get-ChildItem (Join-Path $ScriptDir 'java') -Recurse -Filter *.java | ForEach-Object { $_.FullName }
if (-not $javaSrc) { Fail "tidak ada .java di $ScriptDir\java" }

if ((Invoke-Native 'javac' (@('-cp', $AndroidJar, '-source', '8', '-target', '8', '-d', $ClsDir) + $javaSrc)) -ne 0) { Fail "javac gagal" }

$classFiles = Get-ChildItem $ClsDir -Recurse -Filter *.class | ForEach-Object { $_.FullName }
$d8Args = @('-cp', $R8Jar, 'com.android.tools.r8.D8', '--release', '--min-api', '26',
            '--lib', $AndroidJar, '--output', $DexDir) + $classFiles
if ((Invoke-Native 'java' $d8Args) -ne 0) { Fail "d8 gagal" }
$MenuDex = Join-Path $DexDir 'classes.dex'
if (-not (Test-Path $MenuDex)) { Fail "classes.dex tidak terbentuk" }
Write-Host "  menu.dex OK ($([math]::Round((Get-Item $MenuDex).Length/1KB,1)) KB)"

# ---------------------------------------------------------------- decode
Write-Step "Decode APK (apktool, ~1-3 menit untuk 200MB)"
Remove-Item $WorkDir -Recurse -Force -ErrorAction SilentlyContinue
if ((Invoke-Native 'java' @('-Xmx2g', '-jar', $ApkToolJar, 'd', '-f', '-s', '-o', $WorkDir, $Apk)) -ne 0) { Fail "apktool decode gagal" }
$manifestPath = Join-Path $WorkDir 'AndroidManifest.xml'
if (-not (Test-Path $manifestPath)) { Fail "AndroidManifest.xml tidak ketemu setelah decode" }

# ---------------------------------------------------------------- patch manifest
Write-Step "Patch AndroidManifest.xml (sisip provider auto-start)"
[xml]$mf = Get-Content $manifestPath -Raw
$nsAndroid = 'http://schemas.android.com/apk/res/android'
$appNode = $mf.manifest.application
if (-not $appNode) { Fail "<application> tidak ketemu di manifest" }

$prov = $mf.CreateElement('provider')
$prov.SetAttribute('name', $nsAndroid, 'ph.over.mcgg.MCGGProvider')
$prov.SetAttribute('authorities', $nsAndroid, "$($mf.manifest.package).mcgginit")
$prov.SetAttribute('exported', $nsAndroid, 'false')
$prov.SetAttribute('initOrder', $nsAndroid, '100')
[void]$appNode.AppendChild($prov)
$mf.Save($manifestPath)
Write-Host "  provider ph.over.mcgg.MCGGProvider disisipkan"

# ---------------------------------------------------------------- build balik
Write-Step "Build APK (apktool b)"
$unsignedApk = Join-Path $ScriptDir 'out\unsigned.apk'
New-Item -ItemType Directory -Force -Path (Split-Path $Out -Parent) | Out-Null
Remove-Item $unsignedApk -Force -ErrorAction SilentlyContinue
if ((Invoke-Native 'java' @('-Xmx2g', '-jar', $ApkToolJar, 'b', $WorkDir, '-o', $unsignedApk)) -ne 0 -or -not (Test-Path $unsignedApk)) { Fail "apktool build gagal" }

# ---------------------------------------------------------------- inject dex + so via zip langsung
Write-Step "Inject menu.dex + libmcggmod.so ke APK"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($unsignedApk, 'Update')
try {
    $n = 1
    while ($zip.GetEntry("classes$(if($n -eq 1){''}else{$n}).dex")) { $n++ }
    $dexName = "classes$(if($n -eq 1){''}else{$n}).dex"
    $eDex = $zip.CreateEntry($dexName, [System.IO.Compression.CompressionLevel]::Optimal)
    $ws = $eDex.Open(); $fs = [System.IO.File]::OpenRead($MenuDex)
    $fs.CopyTo($ws); $fs.Dispose(); $ws.Dispose()
    Write-Host "  $dexName ditambahkan"

    $soName = 'lib/arm64-v8a/libmcggmod.so'
    if ($zip.GetEntry($soName)) { $zip.GetEntry($soName).Delete() }
    $eSo = $zip.CreateEntry($soName, [System.IO.Compression.CompressionLevel]::NoCompression)
    $ws2 = $eSo.Open(); $fs2 = [System.IO.File]::OpenRead($Payload)
    $fs2.CopyTo($ws2); $fs2.Dispose(); $ws2.Dispose()
    Write-Host "  $soName ditambahkan (stored, no-compress)"
} finally { $zip.Dispose() }

# ---------------------------------------------------------------- zipalign + sign
Write-Step "Zipalign"
$alignedApk = Join-Path $ScriptDir 'out\aligned.apk'
Remove-Item $alignedApk -Force -ErrorAction SilentlyContinue
if ((Invoke-Native $ZipAlign @('-f', '-p', '4', $unsignedApk, $alignedApk)) -ne 0) { Fail "zipalign gagal" }

Write-Step "Sign (v1+v2)"
if (-not (Test-Path $KeyStore)) {
    Write-Host "  generate keystore baru ..."
    if ((Invoke-Native 'keytool' @('-genkeypair', '-v', '-keystore', $KeyStore, '-alias', $KeyAlias,
        '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000',
        '-storepass', $KeyPass, '-keypass', $KeyPass,
        '-dname', 'CN=MCGG Repack, OU=Mod, O=Local, C=ID')) -ne 0) { Fail "keytool gagal" }
}
Remove-Item $Out -Force -ErrorAction SilentlyContinue
if ((Invoke-Native $ApkSigner @('sign', '--ks', $KeyStore, '--ks-pass', "pass:$KeyPass",
    '--ks-key-alias', $KeyAlias, '--key-pass', "pass:$KeyPass",
    '--v1-signing-enabled', 'true', '--v2-signing-enabled', 'true',
    '--out', $Out, $alignedApk)) -ne 0 -or -not (Test-Path $Out)) { Fail "apksigner gagal" }

# ---------------------------------------------------------------- selesai
Remove-Item $unsignedApk, $alignedApk -Force -ErrorAction SilentlyContinue
Write-Step "SELESAI"
$size = [math]::Round((Get-Item $Out).Length/1MB, 1)
Write-Host "  hasil : $Out ($size MB)" -ForegroundColor Green
Write-Host ""
Write-Host "LANGKAH PAKAI DI HP:" -ForegroundColor Yellow
Write-Host "  1. Uninstall MCGG asli (beda signature, tidak bisa timpa)"
Write-Host "  2. Install APK hasil repack ini"
Write-Host "  3. Jalankan game - menu muncul otomatis (panel bisa di-drag)"
Write-Host ""
Write-Host "CATATAN:" -ForegroundColor Yellow
Write-Host "  - Kalau server tolak login (cek signature), game mungkin tidak bisa online."
Write-Host "  - Update game = harus repack ulang APK versi baru."
