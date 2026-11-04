# Heroes IV: проверка статической рекомпиляции

Отдельный эксперимент. Рабочая ветка `codex/android-arm64-import` не изменяется. Этот каталог не содержит Heroes IV, полученный из нее C или готовый порт игры. Оригинальная игра требуется только для локального эксперимента. Тестовые APK без игрового процесса не создаются.

Сводные результаты находятся в `RESULTS-RU.md`. Источник инструментария и лицензии указаны в `UPSTREAM.md`.

## Воспроизведение на ARM64

Все каталоги с PE, сгенерированным C, образами памяти и исполняемыми результатами должны находиться вне Git checkout, на внешнем диске. Ниже `$poc_private` и `$h5_source` являются локальными путями.

```sh
poc_private=/your/external/disk/h4-static-private
h5_source=/your/external/disk/h5-source
python3 -m venv "$poc_private/venv"
"$poc_private/venv/bin/python" -m pip install -r static-recomp/requirements.txt
git clone --depth 1 --branch v0.3.2-dev https://github.com/phys-winner/pc_decomp_homm5.git "$h5_source"
git -C "$h5_source" rev-parse HEAD
```

Ожидаемый коммит H5: `6dc7914d7529d92beaf323c0f88a5102b4d0fd69`. Скрипт проверяет его перед генерацией. Исходный установщик не исполняется:

```sh
"$poc_private/venv/bin/python" static-recomp/tools/extract_pe.py /your/external/disk/HeroesIV.sh "$poc_private/original-pe"
"$poc_private/venv/bin/python" static-recomp/tools/pe_audit.py "$poc_private/original-pe" "$poc_private/pe-audit.json"
"$poc_private/venv/bin/python" "$h5_source/reference/pcrecomp/tools/disasm/disasm32.py" "$poc_private/original-pe/heroes4.exe" --output "$poc_private/functions.json"
```

Извлечение рассчитано на исследованный LinuxRulez-файл с XZ по смещению 1 328 763; при другом формате оно завершается ошибкой. Имеющиеся файлы не перезаписываются. Версия EXE должна совпасть с SHA-256 в `config/heroes4-3.0wow.json`; название файла само по себе не подтверждает совместимость или происхождение.

```sh
"$poc_private/venv/bin/python" static-recomp/tools/lift_selected.py --h5 "$h5_source" --exe "$poc_private/original-pe/heroes4.exe" --manifest static-recomp/config/heroes4-3.0wow.json --output "$poc_private/lifted"
clang -O2 -fwrapv -fno-strict-aliasing -ffp-contract=off -DRECOMP_GENERATED_CODE -DRECOMP_TRACE -include "$poc_private/lifted/runtime_config.h" -I"$h5_source/android/runtime" "$poc_private/lifted/lifted.c" static-recomp/native/poc_runtime.c -lm -o "$poc_private/native-arm64"
"$poc_private/venv/bin/python" static-recomp/tools/verify_selected.py --h5 "$h5_source" --exe "$poc_private/original-pe/heroes4.exe" --manifest static-recomp/config/heroes4-3.0wow.json --native "$poc_private/native-arm64" --result "$poc_private/result.json"
```

Команды выполняются на ARM64 macOS или Linux. На Mac при необходимости добавьте `-arch arm64`; на другом процессоре runtime H5 намеренно не компилируется. Гостевая память - отдельное окно 4 ГиБ с доступом только к выделенной области; страницы гостевого PE не исполняются. Прототип использует перемещаемую базу окна, поскольку macOS не гарантирует размещение по фиксированной подсказке H5. Это не заменяет тест фиксированной модели адресов полноценного Android-порта.

Проверяются функции по адресам `0x00437040`, `0x00796B30`, `0x008DFFDF` и `0x008E44C3`. Имена в manifest условные, по статическому поведению; отладочных символов оригинальной игры нет. Native-процесс выполняет скомпилированный C. Unicorn используется отдельно только как эталон исполнения входных x86-инструкций для сравнения результатов, стека и памяти.

Генерация C не содержит ручных замен тела этих функций. Неподдерживаемые инструкции получают trap от CheckedLifter, а неизвестные косвенные вызовы завершают процесс с кодом 77. В малом poc_runtime TLS-путь ограничен одним заранее подготовленным контекстом. Новый bootstrap ниже поддерживает динамическую TLS одного гостевого потока; многопоточность не реализована.

## Оригинальный startup и меню

Малый `poc_runtime` намеренно содержит только минимальный host и отрицательный тест HeapCreate. Для полного CRT/WinMain используется отдельный `bootstrap` с heap, VM, TLS, файлами и SDL/DirectDraw7. Он не пропускает оригинальный CRT.

Требуются clang/clang++, SDL3 через pkg-config, Python-зависимости и настоящий mapped PE image. На Linux нужен C++23 std::expected (в CI g++-14). Все большие файлы и TMPDIR сборщика размещаются возле приватного выходного бинарника.

```sh
export H4_GAME_ROOT=/your/external/disk/resources/HeroesIV-Complete
export H4_FRAME_OUTPUT="$poc_private/original-frame.bmp"
# Только тестовый ввод: Escape после вступления, без изменения инструкций игры.
export H4_SMOKE_ESCAPE=1
"$poc_private/venv/bin/python" static-recomp/tools/advance_startup.py --h5 "$h5_source" --exe "$poc_private/original-pe/heroes4.exe" --manifest static-recomp/config/heroes4-3.0wow.json --image "$poc_private/heroes4-image.bin" --work "$poc_private/startup-next" --catalog "$poc_private/functions.json" --steps 100
```

Mapped image заранее создается локально через `pefile.PE(...).get_memory_mapped_image()`. Скрипт проверяет его совпадение с PE. Для штатного New Game после меню можно дополнительно включить `H4_SMOKE_NEW_GAME=1`: это тестовая WM_COMMAND 32897 из исходного RT_MENU, не доказательство физического ввода. В `--work` сохраняется расширяемый manifest: неизвестный адрес проверяется по executable section, затем добавляется ограниченное замыкание прямых вызовов из каталога. Повтор команды продолжает тот же набор. Для первого прохода через статические инициализаторы могут потребоваться дополнительные итерации. Это средство исследования, не готовый лаунчер.

Скрипт сохраняет полные локальные журналы и прекращает запуск после 20 секунд. Timeout в меню является границей наблюдения, не доказательством завершения сценария. Для прямого запуска собранного результата:

```sh
export H4_STATE_DIR="$poc_private/startup-next/userdata"
"$poc_private/startup-next/startup" "$poc_private/heroes4-image.bin"
```

Ресурсы доступны гостю только для чтения как `C:\HeroesIV`. Записи и реестр изолированы в H4_STATE_DIR. Не добавляйте в Git приватные manifest/catalog с дизассемблированием, generated C, исходный PE/DLL, бинарники, H4R или оригинальные кадры.

Подтверждены оригинальный WinMain `0x008D2580`, WndProc `0x008D14E0`, чтение девяти H4R и главное меню Winds of War на ARM64 Mac. Звук/вступительное видео недоступны и возвращают ошибку. При штатной команде New Game исходный код бросает C++ исключение bool; пока отсутствует SEH/EH dispatch до оригинального catch. Выбор сценария, карта и многопоточность еще не подтверждены. Неизвестные API/переходы прекращают выполнение с диагностикой; успешный ответ вместо отсутствующей реализации не подставляется.

## Публичные проверки

Workflow `.github/workflows/h4-static-recomp.yml` не получает оригинальную игру. Он создает собственный PE32 из `tests/make_fixture.py`, проверяет C на настоящем Linux ARM64 runner, затем отдельно собирает этот же синтетический пример Android NDK 29 для `aarch64-linux-android30`. Вторая сборка не является запуском Android-приложения.

Синтетические тесты покрывают вычисления, условные переходы, циклы, запись в память, Win32 TLS/ошибку, стек и x87. Отдельный тест требует явной остановки при неподдерживаемом импорте. Для DirectDraw-поверхностей проверяются pitch, палитра, Lock/Unlock, clipping, color key, перекрывающийся Blt и Flip. SDL3 собирается из закрепленного коммита; `--sdl` обязан реально представить кадры и завершается ошибкой, если SDL выключен.

`native/ddraw_surface.*` содержит общие примитивы поверхностей. Новый `ddraw_com.c` и `surface_bridge.cpp` связывают их с настоящим DirectDrawCreateEx, 32-битными COM-vtables и SDL. `win32_window.c`, `gdi.c`, `files.c`, `registry.c`, `pe_version.c` обеспечивают проверенную часть host. Публичный CI рисует только свои тестовые пиксели; оригинальное меню проверялось отдельно локально.
