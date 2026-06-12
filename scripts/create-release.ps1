param(
    [string]$Version = "V1.1.1",
    [string]$WindowsInstallerPath = "dist\installer\LiJointMaster3-V1.1.0-Setup.exe",
    [string]$WindowsPortableDir = "dist\windows",
    [string]$LinuxPackagePath = "",
    [string]$LinuxPortableDir = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$releaseRoot = Join-Path $projectRoot ("dist\releases\" + $Version)
$sourceRoot = Join-Path $releaseRoot "source"
$windowsRoot = Join-Path $releaseRoot "windows"
$linuxRoot = Join-Path $releaseRoot "linux"
$tempRoot = Join-Path $projectRoot "dist\.release-staging"
$sourceStageDir = Join-Path $tempRoot ("LiJointMaster3-" + $Version + "-source")
$sourceZip = Join-Path $sourceRoot ("LiJointMaster3-" + $Version + "-source.zip")
$sourceTarGz = Join-Path $sourceRoot ("LiJointMaster3-" + $Version + "-source.tar.gz")
$releaseNotesPath = Join-Path $releaseRoot "RELEASE_NOTES.md"

$excludeDirNames = @(".git", "build", "dist", ".qtcreator", ".vscode")
$excludeFilePatterns = @("*.log", "*.obj", "*.o", "*.exe", "*.dll", "*.a", "*.lib", "*.exp", "*.ilk", "*.pdb")

function Remove-IfExists {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Copy-ReleaseTree {
    param(
        [string]$Source,
        [string]$Destination
    )

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null

    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        if ($excludeDirNames -contains $_.Name) {
            return
        }

        $targetPath = Join-Path $Destination $_.Name

        if ($_.PSIsContainer) {
            Copy-ReleaseTree -Source $_.FullName -Destination $targetPath
            return
        }

        foreach ($pattern in $excludeFilePatterns) {
            if ($_.Name -like $pattern) {
                return
            }
        }

        Copy-Item -LiteralPath $_.FullName -Destination $targetPath -Force
    }
}

Remove-IfExists $releaseRoot
Remove-IfExists $tempRoot

New-Item -ItemType Directory -Path $sourceRoot, $windowsRoot, $linuxRoot, $tempRoot -Force | Out-Null

Copy-ReleaseTree -Source $projectRoot -Destination $sourceStageDir

if (Test-Path -LiteralPath $sourceZip) {
    Remove-Item -LiteralPath $sourceZip -Force
}
Compress-Archive -Path $sourceStageDir -DestinationPath $sourceZip -Force

$tarCommand = Get-Command tar.exe -ErrorAction SilentlyContinue
if ($tarCommand) {
    & $tarCommand.Source -a -c -f $sourceTarGz -C $tempRoot (Split-Path $sourceStageDir -Leaf)
}

$installerFullPath = Join-Path $projectRoot $WindowsInstallerPath
if (Test-Path -LiteralPath $installerFullPath) {
    Copy-Item -LiteralPath $installerFullPath -Destination (Join-Path $windowsRoot ("LiJointMaster3-" + $Version + "-windows-x64-setup.exe")) -Force
}

$portableFullPath = Join-Path $projectRoot $WindowsPortableDir
if (Test-Path -LiteralPath $portableFullPath) {
    Copy-Item -LiteralPath $portableFullPath -Destination (Join-Path $windowsRoot ("LiJointMaster3-" + $Version + "-windows-x64-portable")) -Recurse -Force
}

if (-not [string]::IsNullOrWhiteSpace($LinuxPackagePath)) {
    $linuxPackageFullPath = Join-Path $projectRoot $LinuxPackagePath
    if (Test-Path -LiteralPath $linuxPackageFullPath) {
        Copy-Item -LiteralPath $linuxPackageFullPath -Destination (Join-Path $linuxRoot (Split-Path $linuxPackageFullPath -Leaf)) -Force
    }
}

if (-not [string]::IsNullOrWhiteSpace($LinuxPortableDir)) {
    $linuxPortableFullPath = Join-Path $projectRoot $LinuxPortableDir
    if (Test-Path -LiteralPath $linuxPortableFullPath) {
        Copy-Item -LiteralPath $linuxPortableFullPath -Destination (Join-Path $linuxRoot ("LiJointMaster3-" + $Version + "-linux-portable")) -Recurse -Force
    }
}

if (-not (Get-ChildItem -LiteralPath $linuxRoot -Force | Select-Object -First 1)) {
    @"
# Linux Artifacts

Place Linux release artifacts for $Version in this folder, for example:
- AppImage
- tar.gz portable package
- distro-specific install package
"@ | Set-Content -LiteralPath (Join-Path $linuxRoot "README.md") -Encoding UTF8
}

@"
# LiJointMaster3 $Version

## Included Artifacts

- source/: source release archives
- windows/: Windows installer and portable package
- linux/: Linux release assets or staging placeholder

## Notes

- Version source: CMakeLists.txt and packaging/LiJointMaster3.iss
- Windows installer source: $WindowsInstallerPath
- Windows portable source: $WindowsPortableDir
"@ | Set-Content -LiteralPath $releaseNotesPath -Encoding UTF8

Remove-IfExists $tempRoot

Write-Host "Release prepared at: $releaseRoot"
