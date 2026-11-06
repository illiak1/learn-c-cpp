# CMakeLists.txt - cross-platform build (Linux, Windows, macOS).
# Raylib is downloaded with FetchContent at the pinned release tag.
cmake_minimum_required(VERSION 3.22)
project(learn_c_cpp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 6.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(learn-c-cpp ${APP_SOURCES})
target_include_directories(learn-c-cpp PRIVATE src)
target_link_libraries(learn-c-cpp PRIVATE raylib)
if(MSVC)
  target_compile_options(learn-c-cpp PRIVATE /W4 /utf-8)
  set_target_properties(learn-c-cpp PROPERTIES WIN32_EXECUTABLE FALSE)
else()
  target_compile_options(learn-c-cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy assets and content next to the executable so it runs from the build dir.
add_custom_command(TARGET learn-c-cpp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:learn-c-cpp>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/content $<TARGET_FILE_DIR:learn-c-cpp>/content
)

# Unit tests: Raylib-free logic only (parser, answer checking, exam builder).
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/content/*.cpp src/logic/*.cpp)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
if(TEST_SOURCES)
  enable_testing()
  add_executable(unit-tests ${CORE_SOURCES} ${TEST_SOURCES})
  target_include_directories(unit-tests PRIVATE src)
  add_test(NAME unit-tests COMMAND unit-tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
