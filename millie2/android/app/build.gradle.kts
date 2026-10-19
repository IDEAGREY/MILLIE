plugins { id("com.android.application") }
android {
    namespace = "com.millie.two"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.millie.civops2"
        testInstrumentationRunner = "com.millie.two.LinkAuditInstrumentation"
        minSdk = 26
        targetSdk = 35
        versionCode = 20014
        versionName = "2.0.14"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildTypes { release { isMinifyEnabled = false } }
}
dependencies {
    implementation("com.github.mik3y:usb-serial-for-android:3.11.0")
}
