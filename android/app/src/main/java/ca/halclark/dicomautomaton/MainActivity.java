package ca.halclark.dicomautomaton;

import com.google.androidgamesdk.GameActivity;

/**
 * MainActivity for DICOMautomaton SDL Viewer.
 *
 * This activity extends GameActivity (from the Android Game Development Kit),
 * which provides a full-screen NativeActivity-compatible surface optimised for
 * C++ game/graphics applications.  The actual SDL_Viewer logic lives entirely
 * in the native library declared in AndroidManifest.xml
 * (android.app.lib_name = "dcma_sdl_viewer").
 *
 * GameActivity automatically:
 *   - Loads the shared library named by android.app.lib_name.
 *   - Forwards Android lifecycle events to the native side via the
 *     android_main() / SDL_main() entry point.
 *   - Manages the ANativeWindow surface and input events.
 */
public class MainActivity extends GameActivity {
    // No additional Java code is needed for the MVP.
    // GameActivity handles all lifecycle forwarding to the native layer.
    // SDL's Android glue (SDL_android.c) connects SDL_main() to GameActivity.
}
