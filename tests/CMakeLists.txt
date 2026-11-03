add_executable(h4-combat-tests battle_main.cpp battle_cases.cpp)
target_link_libraries(h4-combat-tests PRIVATE h4::rules)
add_test(NAME combat COMMAND h4-combat-tests)
