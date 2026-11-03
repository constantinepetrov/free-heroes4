package org.openh4.android;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.*;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

public final class LauncherActivity extends Activity {
    private TextView status;
    private Button importButton, startButton;
    private volatile boolean importing;
    private static final int PICK_FOLDER = 1;
    private static final Set<String> ARCHIVES = Set.of("heroes4.h4r", "new.h4r", "x2_updates.h4r",
        "x2 updates.h4r", "x2_override.h4r", "x2 override.h4r", "storm_updates.h4r",
        "storm updates.h4r", "storm_override.h4r", "storm override.h4r", "updates.h4r",
        "text.h4r", "music.h4r", "movies.h4r", "x2.h4r", "storm.h4r");
    static {
        System.loadLibrary("c++_shared");
        System.loadLibrary("SDL3");
        System.loadLibrary("main");
    }
    static native String validateResources(String dataPath);
    private File game() { return new File(getFilesDir(), "game"); }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        try { recoverImport(); } catch (IOException e) { Log.e("OpenH4", "Import recovery", e); }
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(24 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad, pad, pad);
        TextView title = new TextView(this);
        title.setText("OpenH4 - экспериментальный Android-порт"); title.setTextSize(24);
        layout.addView(title);
        TextView instructions = new TextView(this);
        instructions.setText("Выберите свою папку Heroes IV с Data и maps или подготовленную папку ресурсов. Файлы будут скопированы в память приложения. Исходная папка не изменяется.\n\nВ движке пока отсутствуют бои, игровой ИИ, сохранения и звук.");
        layout.addView(instructions);
        importButton = new Button(this); importButton.setText("Импортировать папку");
        importButton.setOnClickListener(v -> startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION), PICK_FOLDER));
        layout.addView(importButton);
        startButton = new Button(this); startButton.setText("Запустить OpenH4");
        startButton.setEnabled(new File(game(), "data/heroes4.h4r").isFile());
        startButton.setOnClickListener(v -> startActivity(new Intent(this, GameActivity.class)));
        layout.addView(startButton);
        status = new TextView(this); status.setTextIsSelectable(true);
        status.setText(startButton.isEnabled() ? "Ресурсы импортированы. Можно запустить движок." : "Ресурсы ещё не импортированы.");
        ScrollView scroll = new ScrollView(this); scroll.addView(status); layout.addView(scroll);
        setContentView(layout);
        // Keep controls out of the Fold display cutout and Android 15+ edge-to-edge system bars.
        layout.setOnApplyWindowInsetsListener((v, insets) -> {
            android.graphics.Insets bars = insets.getInsets(android.view.WindowInsets.Type.systemBars() | android.view.WindowInsets.Type.displayCutout());
            v.setPadding(pad + bars.left, pad + bars.top, pad + bars.right, pad + bars.bottom);
            return insets;
        });
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_FOLDER || result != RESULT_OK || data == null || importing) return;
        Uri uri = data.getData(); if (uri == null) return;
        importing = true; importButton.setEnabled(false); startButton.setEnabled(false);
        new Thread(() -> {
            try { importTree(uri); show("Импорт завершён. База правил и оригинальное меню проверены движком."); }
            catch (Exception e) { show("Ошибка импорта: " + e.getMessage()); Log.e("OpenH4", "Import failed", e); }
            finally { runOnUiThread(() -> {
                importing = false; importButton.setEnabled(true);
                startButton.setEnabled(new File(game(), "data/heroes4.h4r").isFile());
            }); }
        }, "resource-import").start();
    }
    private void show(String message) {
        Log.i("OpenH4", message);
        try (FileWriter log = new FileWriter(new File(getFilesDir(), "import.log"), true)) {
            log.write(new java.util.Date() + " " + message + "\n");
        } catch (IOException e) { Log.e("OpenH4", "Cannot write log", e); }
        runOnUiThread(() -> { if (!isFinishing()) status.setText(message); });
    }
    private void recoverImport() throws IOException {
        File previous = new File(getFilesDir(), "game-previous");
        if (!game().exists() && previous.exists() && !previous.renameTo(game()))
            throw new IOException("Не удалось восстановить предыдущий импорт");
    }
    private void importTree(Uri tree) throws IOException {
        recoverImport();
        File staging = new File(getFilesDir(), "import-staging");
        remove(staging); if (!staging.mkdirs()) throw new IOException("Не удалось создать временную папку");
        Set<String> names = new HashSet<>();
        try {
            copyTree(tree, DocumentsContract.getTreeDocumentId(tree), "", staging, names, new int[]{0}, 0);
            if (!new File(staging, "data/heroes4.h4r").isFile())
                throw new IOException("Не найден Data/heroes4.h4r. Выберите корневую папку игры.");
            String error = validateResources(new File(staging, "data").getAbsolutePath());
            if (error != null) throw new IOException(error);
            File previous = new File(getFilesDir(), "game-previous");
            remove(previous);
            boolean hadGame = game().exists();
            if (hadGame && !game().renameTo(previous)) throw new IOException("Не удалось сохранить предыдущий импорт");
            if (!staging.renameTo(game())) {
                if (hadGame && !previous.renameTo(game())) throw new IOException("Сбой переноса. Старые файлы в game-previous");
                throw new IOException("Не удалось завершить импорт");
            }
            remove(previous);
        } finally { remove(staging); }
    }
    private void copyTree(Uri tree, String id, String relative, File dest, Set<String> names, int[] visited, int depth) throws IOException {
        if (depth > 12) throw new IOException("Слишком глубокая структура папок");
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
        String[] cols = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE};
        try (Cursor cursor = getContentResolver().query(children, cols, null, null, null)) {
            if (cursor == null) throw new IOException("Папка недоступна");
            while (cursor.moveToNext()) {
                if (++visited[0] > 20000) throw new IOException("Слишком много файлов");
                String childId = cursor.getString(0), name = cursor.getString(1);
                if (name == null || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\"))
                    throw new IOException("Недопустимое имя файла");
                String path = relative + name;
                boolean directory = DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2));
                if (directory) {
                    if (relative.isEmpty() && !(name.equalsIgnoreCase("data") || name.equalsIgnoreCase("maps"))) continue;
                    copyTree(tree, childId, path + "/", dest, names, visited, depth + 1); continue;
                }
                String target = resourcePath(path);
                if (target == null) continue;
                if (!names.add(target.toLowerCase(Locale.ROOT))) throw new IOException("Повторяющийся ресурс: " + target);
                File output = new File(dest, target);
                if (!output.getCanonicalPath().startsWith(dest.getCanonicalPath() + File.separator)) throw new IOException("Недопустимый путь");
                if (!output.getParentFile().isDirectory() && !output.getParentFile().mkdirs()) throw new IOException("Нет доступа для записи");
                show("Копирование: " + target);
                Uri source = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                long count = 0, declared = cursor.isNull(3) ? -1 : cursor.getLong(3);
                try (InputStream input = getContentResolver().openInputStream(source); OutputStream out = new FileOutputStream(output)) {
                    if (input == null) throw new IOException("Не удалось открыть " + name);
                    byte[] buffer = new byte[1024 * 1024]; int n;
                    while ((n = input.read(buffer)) != -1) {
                        if (getFilesDir().getUsableSpace() < n + 32L * 1024 * 1024) throw new IOException("Недостаточно свободного места");
                        out.write(buffer, 0, n); count += n;
                    }
                }
                if (declared >= 0 && count != declared) throw new IOException("Файл скопирован не полностью: " + target);
            }
        }
    }
    static String resourcePath(String path) {
        for (String part : path.split("/", -1))
            if (part.isEmpty() || part.equals(".") || part.equals("..") || part.startsWith("._") || part.contains("\\")) return null;
        String lower = path.toLowerCase(Locale.ROOT);
        if (lower.startsWith("data/") && lower.indexOf('/', 5) == -1 && ARCHIVES.contains(lower.substring(5))) return lower;
        if (lower.startsWith("maps/") && lower.endsWith(".h4c")) return "maps/" + path.substring(5);
        return null;
    }
    private static void remove(File f) throws IOException {
        if (!f.exists()) return;
        File[] files = f.listFiles();
        if (files != null) for (File c : files) remove(c);
        Files.delete(f.toPath());
    }
}
