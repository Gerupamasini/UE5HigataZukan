# Standalone build of the engine-independent tide core, checked against the TypeScript fixtures.
#   cmake -S tests/tide -B build/tide && cmake --build build/tide && ctest --test-dir build/tide
cmake_minimum_required(VERSION 3.16)
project(HigataTideGolden CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TIDE_MODULE ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/HigataTide)

foreach(TEST_NAME tide_golden_test sun_golden_test)
  add_executable(${TEST_NAME}
    ${TEST_NAME}.cpp
    ${TIDE_MODULE}/Private/TideCore/TideCore.cpp
    ${TIDE_MODULE}/Private/TideCore/SunCore.cpp)
  target_include_directories(${TEST_NAME} PRIVATE ${TIDE_MODULE}/Public)
  if(MSVC)
    target_compile_options(${TEST_NAME} PRIVATE /W4 /WX)
  else()
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wshadow -Wconversion -Werror)
  endif()
endforeach()

enable_testing()
file(GLOB FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.txt)
add_test(NAME tide_golden COMMAND tide_golden_test ${FIXTURES})
add_test(NAME sun_golden COMMAND sun_golden_test ${CMAKE_CURRENT_SOURCE_DIR}/sun/sun.txt)
