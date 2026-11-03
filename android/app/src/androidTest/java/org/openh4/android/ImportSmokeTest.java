package org.openh4.android;
import android.app.Instrumentation;
import android.app.Activity;
import android.os.Bundle;
import java.io.File;
import java.nio.file.Files;
public final class ImportSmokeTest extends Instrumentation {
    private boolean performance;
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); performance = arguments != null && "true".equals(arguments.getString("performance")); start(); }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            if (!"data/heroes4.h4r".equals(LauncherActivity.resourcePath("Data/Heroes4.h4r"))) throw new AssertionError("Archive case");
            if (LauncherActivity.resourcePath("Data/res_mod.h4r") != null) throw new AssertionError("Unknown mod");
            if (LauncherActivity.resourcePath("heroes4.exe") != null) throw new AssertionError("Executable copied");
            if (LauncherActivity.resourcePath("maps/._scenario.h4c") != null) throw new AssertionError("AppleDouble copied");
            if (LauncherActivity.resourcePath("maps/../data/scenario.h4c") != null) throw new AssertionError("Traversal accepted");
            String combat = LauncherActivity.combatSelfTest();
            if (combat != null) throw new AssertionError("Combat: " + combat);
            File dir = new File(getTargetContext().getCacheDir(), "invalid-test");
            dir.mkdirs();
            Files.write(new File(dir, "heroes4.h4r").toPath(), new byte[]{'H','4','R',5});
            String error = LauncherActivity.validateResources(dir.getAbsolutePath());
            if (error == null || !error.contains("header")) throw new AssertionError("Truncated H4R accepted: " + error);
            Files.delete(new File(dir, "heroes4.h4r").toPath()); Files.delete(dir.toPath());
            if (performance) {
                File report = new File(getTargetContext().getFilesDir(), "render-benchmark.json");
                Files.deleteIfExists(report.toPath());
                android.content.Intent intent = new android.content.Intent(getTargetContext(), GameActivity.class)
                    .putExtra("renderBenchmark", true).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK);
                // Instrumentation can synchronously launch only its own process.
                startActivitySync(new android.content.Intent(getTargetContext(), LauncherActivity.class)
                    .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK));
                getTargetContext().startActivity(intent);
                long deadline = android.os.SystemClock.uptimeMillis() + 480000;
                while (!report.isFile() && android.os.SystemClock.uptimeMillis() < deadline) Thread.sleep(200);
                if (!report.isFile()) throw new AssertionError("Renderer benchmark timed out");
                // Wait for the native writer to close before exposing the result.
                Thread.sleep(300);
                String json = new String(Files.readAllBytes(report.toPath()), java.nio.charset.StandardCharsets.UTF_8);
                if (!json.startsWith("[")) throw new AssertionError("Benchmark: " + json);
                result.putString("stream", "BENCHMARK " + json + "\nPASS: renderer benchmark\n");
                finish(Activity.RESULT_OK, result); return;
            }
            result.putString("stream", "PASS: resource whitelist, case normalization, native H4R rejection; combat mechanics and adventure settlement\n");
            finish(Activity.RESULT_OK, result);
        } catch (Throwable e) {
            result.putString("stream", "FAIL: " + e + "\n"); finish(Activity.RESULT_CANCELED, result);
        }
    }
}
