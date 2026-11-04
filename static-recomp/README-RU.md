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

Генерация C не содержит ручных замен тела этих функций. Неподдерживаемые инструкции получают trap от CheckedLifter, а неизвестные косвенные вызовы завершают процесс с кодом 77. TLS-путь ограничен одним заранее подготовленным контекстом; многопоточность и создание TLS при первом обращении не реализованы.

`startup` дополнительно включает CRT-вход `0x008E3981` и инициализацию heap `0x008E96A5`. Для него создайте локальный mapped image через `pefile.PE(...).get_memory_mapped_image()` и передайте путь третьим аргументом native-программы. Это именно mapped image, не исходный EXE. Импорты привязываются к именованным ловушкам, кроме реализованного `GetVersion`. Проверенный ожидаемый результат - остановка на `KERNEL32.dll!HeapCreate`; успешного WinMain еще нет.

## Публичные проверки

Workflow `.github/workflows/h4-static-recomp.yml` не получает оригинальную игру. Он создает собственный PE32 из `tests/make_fixture.py`, проверяет C на настоящем Linux ARM64 runner, затем отдельно собирает этот же синтетический пример Android NDK 29 для `aarch64-linux-android30`. Вторая сборка не является запуском Android-приложения.

Синтетические тесты покрывают вычисления, условные переходы, циклы, запись в память, Win32 TLS/ошибку, стек и x87. Отдельный тест требует явной остановки при неподдерживаемом импорте. Для DirectDraw-поверхностей проверяются pitch, палитра, Lock/Unlock, clipping, color key, перекрывающийся Blt и Flip. SDL3 собирается из закрепленного коммита; `--sdl` обязан реально представить кадры и завершается ошибкой, если SDL выключен.

`native/ddraw_surface.*` - нижний слой поверхностей с выводом через существующий `h4::platform::Window`. Полного `DirectDrawCreateEx`, COM-vtable/guest-pointer bridge, GDI и оконных сообщений в нем пока нет. Успешный цветовой тест не означает, что оригинальная игра уже рисует через этот слой.
