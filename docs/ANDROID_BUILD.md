# Hamun Android Build

The Android path is intentionally designed around one command.

## Windows

```bat
BuildAndroid.bat
```

## Linux / macOS

```sh
./build-android.sh
```

The helper will:

1. find Android Studio's SDK automatically
2. verify/install the required SDK, Build Tools, NDK and CMake packages
3. download a private Gradle distribution into `.hamun/tools` if Gradle is not installed
4. configure the Android project
5. build the native Hamun C++ code through the same root CMake project
6. package an APK
7. copy the result to `dist/android/HamunGame-debug.apk`

To install and launch directly on a USB-connected device:

```bat
BuildAndroid.bat --install
```

Release build:

```bat
BuildAndroid.bat --release
```

## Pinned toolchain

- Android Gradle Plugin: 9.4.0
- Gradle: 9.6.0
- compileSdk / targetSdk: 36
- Build Tools: 36.0.0
- NDK: 28.2.13676358
- CMake: 3.22.1
- default ABI: arm64-v8a
- minimum Android API: 26

The default test APK uses Android NativeActivity, so there is no Java/Kotlin
gameplay layer to maintain. The same engine libraries are compiled by CMake.

The current Android bootstrap validates HamunCore, HamunWorld and HamunGraph and
opens a native application lifecycle. Vulkan/GLES surface rendering is the next
Android graphics milestone.
