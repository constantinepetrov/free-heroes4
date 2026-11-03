package org.openh4.android;
import android.app.Instrumentation;
import android.app.Activity;
import android.os.Bundle;
import java.io.File;
import java.nio.file.Files;
public final class ImportSmokeTest extends Instrumentation {
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            if (!"data/heroes4.h4r".equals(LauncherActivity.resourcePath("Data/Heroes4.h4r"))) throw new AssertionError("Archive case");
            if (LauncherActivity.resourcePath("Data/res_mod.h4r") != null) throw new AssertionError("Unknown mod");
            if (LauncherActivity.resourcePath("heroes4.exe") != null) throw new AssertionError("Executable copied");
            if (LauncherActivity.resourcePath("maps/._scenario.h4c") != null) throw new AssertionError("AppleDouble copied");
            if (LauncherActivity.resourcePath("maps/../data/scenario.h4c") != null) throw new AssertionError("Traversal accepted");
            File dir = new File(getTargetContext().getCacheDir(), "invalid-test");
            dir.mkdirs();
            Files.write(new File(dir, "heroes4.h4r").toPath(), new byte[]{'H','4','R',5});
            String error = LauncherActivity.validateResources(dir.getAbsolutePath());
            if (error == null || !error.contains("header")) throw new AssertionError("Truncated H4R accepted: " + error);
            Files.delete(new File(dir, "heroes4.h4r").toPath()); Files.delete(dir.toPath());
            result.putString("stream", "PASS: resource whitelist, case normalization, native H4R rejection\n");
            finish(Activity.RESULT_OK, result);
        } catch (Throwable e) {
            result.putString("stream", "FAIL: " + e + "\n"); finish(Activity.RESULT_CANCELED, result);
        }
    }
}
