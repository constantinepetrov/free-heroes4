#include "battle_cases.hpp"
#include <cstdio>
#include <exception>
int main() {try {h4::tests::battle_checks();std::puts("PASS: combat mechanics, losses and adventure settlement");return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
