# Release Process

## Versioning

Update the project version in:

- `CMakeLists.txt`
- `packaging/LiJointMaster3.iss`

## Windows Build

From the repository root:

```powershell
.\scripts\package-windows.ps1
```

This produces:

- `dist/windows/`
- `dist/installer/LiJointMaster3-V1.1.0-Setup.exe`

## Formal Release Bundle

After the Windows package is ready:

```powershell
.\scripts\create-release.ps1 -Version V1.1.0
```

This creates:

- `dist/releases/V1.1.0/source/`
- `dist/releases/V1.1.0/windows/`
- `dist/releases/V1.1.0/linux/`

If Linux artifacts already exist, pass them explicitly:

```powershell
.\scripts\create-release.ps1 `
  -Version V1.1.0 `
  -LinuxPackagePath "dist/linux/LiJointMaster3-V1.1.0-linux-x64.tar.gz" `
  -LinuxPortableDir "dist/linux/portable"
```
