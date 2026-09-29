# Apple Build Infrastructure Roadmap

Hamun's Apple path is designed around Metal and 64-bit ARM/x64 host tooling.

Planned one-command targets:

```text
BuildApple.sh macos
BuildApple.sh ios
BuildApple.sh ios-simulator
```

The Apple build foundation will provide:

- CMake/Xcode generation
- macOS application bundle packaging
- iOS device and simulator targets
- Apple resource/asset bundle handling
- Metal backend integration point behind HamunRHI
- Apple Silicon ARM64 as a first-class target
- CI validation on macOS runners

The engine must remain buildable on Windows/Linux/Android when Apple tooling is
not installed. Metal implementation files will therefore be isolated behind
Apple platform guards and separate CMake targets.

Actual iOS device signing, provisioning profiles, App Store packaging, and
console-equivalent restricted SDK integration require the appropriate Apple
developer credentials and host tooling and are intentionally kept outside the
cross-platform core.
