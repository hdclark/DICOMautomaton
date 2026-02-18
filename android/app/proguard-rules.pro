# ProGuard rules for DICOMautomaton Android app.
# The app is almost entirely native code, so there is very little Java to minify.

# Keep GameActivity and its subclasses.
-keep class com.google.androidgamesdk.GameActivity { *; }
-keep class ca.halclark.dicomautomaton.MainActivity { *; }

# Keep JNI entry points called from native code.
-keepclassmembers class * {
    native <methods>;
}
