#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ANDROID_PROJECT = ROOT / "Platforms" / "Android"
CACHE = ROOT / ".hamun" / "tools"
DIST = ROOT / "dist" / "android"

GRADLE_VERSION = "9.6.0"
NDK_VERSION = "28.2.13676358"
CMAKE_VERSION = "3.22.1"
COMPILE_SDK = "37"
BUILD_TOOLS = "36.0.0"

SDK_PACKAGES = [
    "platform-tools",
    f"platforms;android-{COMPILE_SDK}",
    f"build-tools;{BUILD_TOOLS}",
    f"ndk;{NDK_VERSION}",
    f"cmake;{CMAKE_VERSION}",
]


def host_exe(name: str) -> str:
    return name + ".bat" if os.name == "nt" else name


def existing_dir(value: str | None) -> Path | None:
    if not value:
        return None
    path = Path(value).expanduser()
    return path if path.exists() else None


def find_android_sdk() -> Path:
    candidates: list[Path] = []

    for key in ("ANDROID_SDK_ROOT", "ANDROID_HOME"):
        value = existing_dir(os.environ.get(key))
        if value:
            candidates.append(value)

    home = Path.home()
    if os.name == "nt":
        local = os.environ.get("LOCALAPPDATA")
        if local:
            candidates.append(Path(local) / "Android" / "Sdk")
    elif platform.system() == "Darwin":
        candidates.append(home / "Library" / "Android" / "sdk")
    else:
        candidates.append(home / "Android" / "Sdk")

    for path in candidates:
        if path.exists():
            return path.resolve()

    raise RuntimeError(
        "Android SDK not found. Install Android Studio once, or set "
        "ANDROID_SDK_ROOT to your SDK folder."
    )


def find_sdkmanager(sdk: Path) -> Path:
    command = host_exe("sdkmanager")
    candidates = [sdk / "cmdline-tools" / "latest" / "bin" / command]

    root = sdk / "cmdline-tools"
    if root.exists():
        for child in sorted(root.iterdir(), reverse=True):
            candidates.append(child / "bin" / command)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise RuntimeError(
        "sdkmanager was not found. In Android Studio install "
        "'Android SDK Command-line Tools (latest)'."
    )


def find_java_home() -> Path | None:
    configured = existing_dir(os.environ.get("JAVA_HOME"))
    if configured and (configured / "bin" / host_exe("java")).exists():
        return configured

    candidates: list[Path] = []
    if os.name == "nt":
        program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
        candidates.append(
            Path(program_files) / "Android" / "Android Studio" / "jbr"
        )
    elif platform.system() == "Darwin":
        candidates.append(
            Path("/Applications/Android Studio.app/Contents/jbr/Contents/Home")
        )
    else:
        candidates.extend([
            Path("/opt/android-studio/jbr"),
            Path.home() / "android-studio" / "jbr",
        ])

    for candidate in candidates:
        if (candidate / "bin" / host_exe("java")).exists():
            return candidate

    return None


def install_sdk_packages(sdkmanager: Path, sdk: Path) -> None:
    print("[Hamun] Checking Android SDK/NDK packages...")
    env = os.environ.copy()
    env["ANDROID_SDK_ROOT"] = str(sdk)

    process = subprocess.run(
        [str(sdkmanager), *SDK_PACKAGES],
        input=("y\n" * 200),
        text=True,
        env=env,
    )
    if process.returncode != 0:
        raise RuntimeError("Android SDK package installation failed.")


def gradle_executable() -> Path:
    system_gradle = shutil.which("gradle")
    if system_gradle:
        return Path(system_gradle)

    CACHE.mkdir(parents=True, exist_ok=True)
    folder = CACHE / f"gradle-{GRADLE_VERSION}"
    executable = folder / "bin" / host_exe("gradle")
    if executable.exists():
        return executable

    archive = CACHE / f"gradle-{GRADLE_VERSION}-bin.zip"
    if not archive.exists():
        url = (
            f"https://services.gradle.org/distributions/"
            f"gradle-{GRADLE_VERSION}-bin.zip"
        )
        print(f"[Hamun] Downloading Gradle {GRADLE_VERSION}...")
        urllib.request.urlretrieve(url, archive)

    print("[Hamun] Extracting Gradle...")
    with zipfile.ZipFile(archive, "r") as zip_file:
        zip_file.extractall(CACHE)

    if not executable.exists():
        raise RuntimeError("Gradle extraction did not produce an executable.")
    return executable


def write_local_properties(sdk: Path) -> None:
    value = str(sdk).replace("\\", "\\\\")
    (ANDROID_PROJECT / "local.properties").write_text(
        f"sdk.dir={value}\n",
        encoding="utf-8",
    )


def copy_apk(variant: str) -> Path:
    lower = variant.lower()
    source = (
        ANDROID_PROJECT
        / "app"
        / "build"
        / "outputs"
        / "apk"
        / lower
        / f"app-{lower}.apk"
    )

    if not source.exists():
        raise RuntimeError(f"APK was not produced at: {source}")

    DIST.mkdir(parents=True, exist_ok=True)
    destination = DIST / f"HamunGame-{lower}.apk"
    shutil.copy2(source, destination)
    return destination


def install_apk(sdk: Path, apk: Path) -> None:
    adb = sdk / "platform-tools" / host_exe("adb")
    if not adb.exists():
        raise RuntimeError("adb was not found after installing platform-tools.")

    print("[Hamun] Installing APK on connected Android device...")
    subprocess.run([str(adb), "install", "-r", str(apk)], check=True)
    subprocess.run([
        str(adb),
        "shell",
        "am",
        "start",
        "-n",
        "com.bdfr.hamun/android.app.NativeActivity",
    ], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="One-command Android build for BDFR Hamun Engine."
    )
    parser.add_argument(
        "--release",
        action="store_true",
        help="Build release variant instead of debug.",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="Install and launch the resulting APK on a connected device.",
    )
    parser.add_argument(
        "--skip-sdk-install",
        action="store_true",
        help="Do not ask sdkmanager to install/verify required packages.",
    )
    args = parser.parse_args()

    try:
        sdk = find_android_sdk()
        sdkmanager = find_sdkmanager(sdk)

        if not args.skip_sdk_install:
            install_sdk_packages(sdkmanager, sdk)

        gradle = gradle_executable()
        write_local_properties(sdk)

        env = os.environ.copy()
        env["ANDROID_SDK_ROOT"] = str(sdk)

        java_home = find_java_home()
        if java_home:
            env["JAVA_HOME"] = str(java_home)

        variant = "Release" if args.release else "Debug"
        task = f":app:assemble{variant}"

        print(f"[Hamun] Building Android {variant} APK...")
        subprocess.run(
            [str(gradle), "--no-daemon", task],
            cwd=ANDROID_PROJECT,
            env=env,
            check=True,
        )

        apk = copy_apk(variant)
        print(f"[Hamun] APK ready: {apk}")

        if args.install:
            install_apk(sdk, apk)

        return 0
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"[Hamun] ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
