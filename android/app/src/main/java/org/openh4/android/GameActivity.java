package org.openh4.android;

import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[]{"c++_shared", "SDL3", "main"}; }
    @Override protected String[] getArguments() {
        return new String[]{"--data", new java.io.File(getFilesDir(), "game/data").getAbsolutePath(), "--resolution", "1024"};
    }
}
