plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.yadavnikhil03.gameunlocker"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.yadavnikhil03.gameunlocker"
        minSdk = 24
        targetSdk = 34
        versionCode = 211
        versionName = "2.1.1"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    applicationVariants.all {
        val variant = this
        val copyWebRootTask = tasks.register<Copy>("copyWebRootToAssets${variant.name.capitalize()}") {
            from(rootProject.file("webroot/index.html"))
            into(project.file("src/main/assets"))
        }
        variant.mergeAssetsProvider.configure {
            dependsOn(copyWebRootTask)
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("com.github.topjohnwu.libsu:core:5.2.2")
}
