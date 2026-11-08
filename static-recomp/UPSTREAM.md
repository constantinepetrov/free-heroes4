# Использованные открытые инструменты

Heroes V: https://github.com/phys-winner/pc_decomp_homm5/tree/6dc7914d7529d92beaf323c0f88a5102b4d0fd69

Используется отдельный checkout этого коммита. Его checkout не изменяется: `tools/lift_game.py` (проверенный генератор, GPL-3.0-or-later), `reference/pcrecomp/tools/` (PE-анализ, CFG и lifter, MIT), `android/runtime/recomp_types.h` (ARM64 runtime, содержит уведомления об исходном MIT-коде и проектные изменения GPL-3.0-or-later). Лицензии и THIRD_PARTY_NOTICES.md сохраняются в исходном checkout.

Собственный тестовый host и скрипты входят в GPL-3.0-проект OpenH4. Сведения о версиях, адресах и хешах не предоставляют прав на коммерческую игру. EXE/DLL, образы PE, C и машинный код, полученные из игры, хранятся только вне этого репозитория. Публичный CI получает исключительно собственные синтетические тесты.

В `native/guest_memory.c` адаптирован `android/host/memory.c`: динамическое размещение окна на macOS, обнуление decommit, проверки диапазонов, повторное использование heap-слотов. В `native/guest_strings.c` сохранены открытые таблицы CP1251/CP1252 из `android/host/strings.c`; классификация CTYPE1 в bootstrap заимствована из kernel32.c. Лицензия этих проектных файлов GPL-3.0-or-later. Заголовок лицензии dlmalloc сохраняется в подключаемом `reference/dlmalloc/malloc.c` (MIT-0); файл используется из закрепленного checkout, в репозиторий не скопирован.

В `native/win32_window.c` схема `h4_call_guest` адаптирована из `android/host/threads.c` H5 (`void_call_guest`): x86 stdcall, гостевой стек и сохранение callee-saved регистров. Лицензия GPL-3.0-or-later.

В `native/eh_runtime.c` адаптированы механизмы `android/host/eh.c` из закрепленного Heroes V, основанные на pcrecomp `runtime/native32/eh32.c` (MIT, ревизия `904ac3129d0c9d05efbeec19a1dce6abbe7f5d26`). Лицензионные уведомления этих проектов сохраняются в их checkout.
