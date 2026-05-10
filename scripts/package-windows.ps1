param(
    [string]$BuildDir = "build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug",
    [string]$QtBinDir = "D:\down\QT\6.11.0\mingw_64\bin",
    [string]$InnoSetupCompiler = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirPath = Join-Path $projectRoot $BuildDir
$exePath = Join-Path $buildDirPath "LiJointMaster3.exe"
$deployDir = Join-Path $projectRoot "dist\windows"
$issPath = Join-Path $projectRoot "packaging\LiJointMaster3.iss"
$windeployqt = Join-Path $QtBinDir "windeployqt.exe"
$mingwBinDir = "D:\down\QT\Tools\mingw1310_64\bin"

if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
    $candidates = @(
        "D:\down\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $InnoSetupCompiler = $candidate
            break
        }
    }

    if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
        $registryEntries = @(
            "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
            "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
            "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*"
        )

        $installLocation = Get-ItemProperty $registryEntries -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "Inno Setup*" } |
            Select-Object -First 1 -ExpandProperty InstallLocation

        if ($installLocation) {
            $registryCompiler = Join-Path $installLocation "ISCC.exe"
            if (Test-Path $registryCompiler) {
                $InnoSetupCompiler = $registryCompiler
            }
        }
    }
}

if (-not (Test-Path $exePath)) {
    throw "未找到可执行文件: $exePath"
}

if (-not (Test-Path $windeployqt)) {
    throw "未找到 windeployqt: $windeployqt"
}

if (Test-Path $deployDir) {
    Remove-Item -LiteralPath $deployDir -Recurse -Force
}
New-Item -ItemType Directory -Path $deployDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination $deployDir -Force

if (Test-Path $mingwBinDir) {
    $env:PATH = "$QtBinDir;$mingwBinDir;$env:PATH"
} else {
    $env:PATH = "$QtBinDir;$env:PATH"
}

& $windeployqt `
    --dir $deployDir `
    --qmldir $projectRoot `
    --compiler-runtime `
    --no-translations `
    (Join-Path $deployDir "LiJointMaster3.exe")

if (-not (Test-Path $InnoSetupCompiler)) {
    Write-Warning "未找到 Inno Setup 编译器: $InnoSetupCompiler"
    Write-Host "Qt 运行目录已生成: $deployDir"
    Write-Host "安装脚本位置: $issPath"
    exit 0
}

& $InnoSetupCompiler $issPath
