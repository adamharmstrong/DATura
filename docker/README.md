# DATura Windows build container

This container compiles DATura and its test executables with MSVC. It does not
run the DATura GUI: Windows containers do not provide the interactive desktop
and Direct3D environment the application requires.

## Prerequisites

- Windows 11 Pro or Enterprise with Windows containers enabled.
- Docker Desktop using the Windows container engine.
- Enough free disk space for the Windows Server Core and Visual Studio Build
  Tools layers.

Check the active engine:

```powershell
docker info --format '{{.OSType}}'
```

The result must be `windows`. If it is `linux`, switch Docker Desktop to Windows
containers before continuing.

## Build the toolchain image

From the repository root:

```powershell
docker build --isolation=hyperv -f Dockerfile.build -t datura-build:local .
```

The default base is Windows Server Core LTSC 2025. On a host that requires a
different compatible base, override it explicitly:

```powershell
docker build --isolation=hyperv --build-arg WINDOWS_BASE_TAG=ltsc2022 -f Dockerfile.build -t datura-build:local .
```

## Compile DATura

Create the output directory on the host, then mount the repository read-only
and the output directory read/write:

```powershell
New-Item -ItemType Directory -Force artifacts\docker-build | Out-Null
$source = (Get-Location).Path
$output = (Resolve-Path artifacts\docker-build).Path
docker run --rm --isolation=hyperv `
  --mount "type=bind,source=$source,target=C:\src,readonly" `
  --mount "type=bind,source=$output,target=C:\out" `
  datura-build:local
```

Build all test projects instead:

```powershell
docker run --rm --isolation=hyperv `
  --mount "type=bind,source=$source,target=C:\src,readonly" `
  --mount "type=bind,source=$output,target=C:\out" `
  datura-build:local powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File C:\DATuraBuild\build.ps1 -Target Tests
```

Use `-Target All` to compile the application followed by every test project,
and `-Configuration Release` for release builds.

Tests that require installed FFXI assets or a Direct3D device should be run on
the Windows host. Do not copy game data into the image.
