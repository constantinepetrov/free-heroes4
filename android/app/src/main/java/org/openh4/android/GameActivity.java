package org.openh4.android;

import org.libsdl.app.SDLActivity;

public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[]{"c++_shared", "SDL3", "main"}; }
    @Override protected String[] getArguments() {
        // Test-only entry from same-signed instrumentation; this activity is not exported.
        if (getIntent().getBooleanExtra("renderBenchmark", false))
            return new String[]{"--render-benchmark", new java.io.File(getFilesDir(), "render-benchmark.json").getAbsolutePath()};
        return new String[]{"--data", new java.io.File(getFilesDir(), "game/data").getAbsolutePath(), "--resolution", "1024"};
    }
}
