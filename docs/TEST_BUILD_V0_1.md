# Hamun Test v0.1 - Windows Package

The first interactive Windows test build is packaged as a self-contained folder.

## Build locally

From a Visual Studio Developer Command Prompt or a machine with CMake + Visual
Studio C++ tools installed:

```bat
BuildWindowsTest.bat
```

The resulting package is created at:

```text
dist/Hamun_Test_v0.1/
```

It contains:

```text
HamunSandbox.exe
Assets/
Run_DX12.bat
Run_DX11.bat
README_TEST.txt
BUILD_INFO.txt
```

## Running the test

Primary renderer:

```text
Run_DX12.bat
```

Compatibility renderer:

```text
Run_DX11.bat
```

Controls:

- WASD — move
- Q / E — move down / up
- hold right mouse button + move mouse — look
- Shift — sprint
- Esc — exit

## Portable asset lookup

Sandbox assets are copied next to the executable during the CMake build. At
runtime Hamun resolves the scene relative to the executable directory, so the
test package has no dependency on the source checkout or GitHub runner path.

## GitHub Actions artifact

Every successful Windows build packages and uploads:

`Hamun-Test-v0.1-Windows-x64`

The artifact contains the ready-to-run package and can be downloaded directly
from the corresponding GitHub Actions run.
