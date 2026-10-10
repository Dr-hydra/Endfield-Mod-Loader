plugins { id("com.android.application") }
layout.buildDirectory.set(file("../../../build/android/app"))
android {
    namespace = "dev.endfield.modloader"
    compileSdk = 37
    ndkVersion = "27.2.12479018"
    defaultConfig {
        applicationId = "dev.endfield.modloader"
        minSdk = 29; targetSdk = 35; versionCode = 100; versionName = "0.1.0"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild { cmake { arguments += "-DEML_DOBBY_SOURCE=" + (providers.gradleProperty("dobbySource").orNull ?: "") } }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    buildTypes { getByName("release") { isMinifyEnabled = false } }
    packaging { jniLibs { useLegacyPackaging = true } }
}
dependencies { compileOnly("io.github.libxposed:api:102.0.0"); implementation("io.github.libxposed:service:102.0.0") }
