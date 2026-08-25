// SPDX-License-Identifier: GPL-3.0-or-later
import java.util.Properties
plugins { id("com.android.application"); id("org.jetbrains.kotlin.android"); id("org.jetbrains.kotlin.plugin.compose") }

val persistentSigningConfigured = listOf(
    "ANDROID_KEYSTORE_PATH",
    "ANDROID_KEYSTORE_PASSWORD",
    "ANDROID_KEY_ALIAS",
    "ANDROID_KEY_PASSWORD",
).all { providers.environmentVariable(it).isPresent }

val repositoryRoot = rootProject.projectDir.parentFile
val vcpkgTag = "2025.06.13"
val vcpkgRoot = repositoryRoot.resolve(".vcpkg/$vcpkgTag")
val vcpkgInstalled = repositoryRoot.resolve(".vcpkg/installed")
val pinnedNdk = androidComponents.sdkComponents.ndkDirectory.map { it.asFile }
val prepareNativeDependencies = tasks.register<Exec>("prepareNativeDependencies") {
    inputs.files(
        repositoryRoot.resolve("mobile/dependencies/vcpkg.json"),
        repositoryRoot.resolve("mobile/dependencies/build-android-dependencies.sh"),
        repositoryRoot.resolve("mobile/dependencies/triplets/arm64-android.cmake"),
    )
    outputs.dir(vcpkgInstalled.resolve("arm64-android"))
    environment("ANDROID_NDK_HOME", pinnedNdk.get().absolutePath)
    commandLine(repositoryRoot.resolve("mobile/dependencies/build-android-dependencies.sh"))
}
val prepareMobileAssets = tasks.register<Sync>("prepareMobileAssets") {
    from(repositoryRoot.resolve("src/external/rawspeed/data")) { include("cameras.xml") }
    from(repositoryRoot.resolve("data")) { include("noiseprofiles.json", "wb_presets.json") }
    into(layout.buildDirectory.dir("generated/mobileAssets"))
}

android {
    namespace = "org.example.darktableandroid"
    compileSdk = 35
    ndkVersion = "27.2.12479018"
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    defaultConfig {
        applicationId = "org.example.darktableandroid"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild { cmake {
            arguments += listOf(
                "-DANDROID_ABI=arm64-v8a",
                "-DANDROID_PLATFORM=android-26",
                "-DCMAKE_ANDROID_ARCH_ABI=arm64-v8a",
                "-DCMAKE_ANDROID_ARCH=aarch64",
                "-DANDROID_STL=c++_shared",
                "-DCMAKE_TOOLCHAIN_FILE=${vcpkgRoot.resolve("scripts/buildsystems/vcpkg.cmake")}",
                "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${pinnedNdk.get().resolve("build/cmake/android.toolchain.cmake")}",
                "-DVCPKG_TARGET_TRIPLET=arm64-android",
                "-DVCPKG_INSTALLED_DIR=$vcpkgInstalled",
                "-DVCPKG_MANIFEST_MODE=OFF",
            )
            abiFilters += "arm64-v8a"
        } }
    }
    signingConfigs {
        create("productionRelease") {
            val path = providers.environmentVariable("ANDROID_KEYSTORE_PATH").orNull
            if(path != null) storeFile = file(path)
            storePassword = providers.environmentVariable("ANDROID_KEYSTORE_PASSWORD").orNull
            keyAlias = providers.environmentVariable("ANDROID_KEY_ALIAS").orNull
            keyPassword = providers.environmentVariable("ANDROID_KEY_PASSWORD").orNull
        }
    }
    flavorDimensions += "distribution"
    productFlavors {
        create("dev") {
            dimension = "distribution"; applicationIdSuffix = ".dev"; versionNameSuffix = "-dev"
            resValue("string", "app_name", "Mobile RAW Editor Dev (Unofficial)")
        }
        create("production") {
            dimension = "distribution"; resValue("string", "app_name", "Mobile RAW Editor (Unofficial)")
        }
    }
    buildTypes {
        debug { isDebuggable = true }
        release {
            isMinifyEnabled = false // TODO: enable after JNI/Compose keep rules are verified.
            isShrinkResources = false
            ndk { debugSymbolLevel = "SYMBOL_TABLE" }
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    signingConfigs.getByName("productionRelease").let { productionSigning ->
        androidComponents.beforeVariants(androidComponents.selector().withBuildType("release")) { builder ->
            if(builder.productFlavors.any { it.second == "production" } && System.getenv("ANDROID_KEYSTORE_PATH") != null) {
                builder.enable = true
            }
        }
        if(System.getenv("ANDROID_KEYSTORE_PATH") != null) productFlavors.getByName("production").signingConfig = productionSigning
    }
    // Local and pull-request builds use the debug key; development releases use the persistent key when configured.
    productFlavors.getByName("dev").signingConfig = if(persistentSigningConfigured) {
        signingConfigs.getByName("productionRelease")
    } else {
        signingConfigs.getByName("debug")
    }
    externalNativeBuild { cmake { path = file("../../mobile/CMakeLists.txt"); version = "3.22.1" } }
    buildFeatures { compose = true; buildConfig = true }
    sourceSets.getByName("main").assets.srcDir(layout.buildDirectory.dir("generated/mobileAssets"))
    packaging { jniLibs.keepDebugSymbols += "**/libdt_mobile.so" }
}

tasks.configureEach {
    if(name.startsWith("configureCMake") || name.startsWith("buildCMake")) dependsOn(prepareNativeDependencies)
    if(name.startsWith("merge") && name.endsWith("Assets")) dependsOn(prepareMobileAssets)
}

kotlin { jvmToolchain(17) }

dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.04.01"))
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    testImplementation("junit:junit:4.13.2")
}
