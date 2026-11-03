add_executable(h4-combat-tests battle_main.cpp battle_cases.cpp)
target_link_libraries(h4-combat-tests PRIVATE h4::rules)
add_test(NAME combat COMMAND h4-combat-tests)

add_executable(h4-encounter-test encounter_main.cpp)
target_link_libraries(h4-encounter-test PRIVATE h4::app h4::assets h4::io h4::screens h4::graphics)
