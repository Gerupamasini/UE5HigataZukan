# Standalone build of the simulation core (Source/HigataZukan/Private/SimCore), checked against fixtures written
# by the web build (tools/golden/spawn_golden.test.ts).
#   cmake -S tests/sim -B build/sim && cmake --build build/sim && ctest --test-dir build/sim --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(HigataSimGolden CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB SIM_SOURCES ${REPO}/Source/HigataZukan/Private/SimCore/*.cpp)

add_executable(sim_golden_test
  sim_golden_test.cpp
  ${SIM_SOURCES}
  ${REPO}/Source/HigataTide/Private/TideCore/TideCore.cpp
  ${REPO}/Source/HigataTide/Private/TideCore/SunCore.cpp)
target_include_directories(sim_golden_test PRIVATE ${REPO}/Source/HigataZukan/Private ${REPO}/Source/HigataTide/Public)
if(MSVC)
  target_compile_options(sim_golden_test PRIVATE /W4 /WX)
else()
  target_compile_options(sim_golden_test PRIVATE -Wall -Wextra -Wshadow -Wconversion -Werror)
endif()

enable_testing()
add_test(NAME sim_golden COMMAND sim_golden_test ${REPO}/Content/Data ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hashirimizu_spawn.txt)
