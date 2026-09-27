plugins {
    id("com.android.application")
}

android {
    namespace = "com.bdfr.hamun"
    compileSdk = 37
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.bdfr.hamun"
        minSdk = 26
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DHAMUN_BUILD_SANDBOX=OFF",
                    "-DHAMUN_BUILD_ANDROID_GAME=ON",
                    "-DHAMUN_ENABLE_D3D12=OFF",
                    "-DHAMUN_ENABLE_VULKAN=ON",
                    "-DHAMUN_ENABLE_GLES=ON"
                )
                cppFlags += listOf("-std=c++20")
                targets += listOf("HamunAndroidGame")
            }
        }
    }

    buildTypes {
        getByName("debug") {
            isMinifyEnabled = false
        }

        getByName("release") {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../../CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
