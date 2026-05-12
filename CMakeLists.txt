cmake_minimum_required(VERSION 3.16)
project(IronMaze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED)

set(GAME_COMMON_WARNINGS "")
if(MSVC)
    set(GAME_COMMON_WARNINGS /W4 /permissive-)
else()
    set(GAME_COMMON_WARNINGS -Wall -Wextra -Wpedantic)
endif()

add_executable(iron_maze
    src/main.cpp
    src/core/Game.cpp
    src/core/StateMachine.cpp
    src/data/GameData.cpp
    src/systems/AudioSystem.cpp
    src/systems/BitmapFont.cpp
    src/systems/CombatSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/Map.cpp
    src/systems/MathUtils.cpp
    src/systems/MazeGen.cpp
    src/systems/MusicSystem.cpp
    src/systems/ParticleSystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/RenderUtils.cpp
    src/systems/SaveSystem.cpp
    src/systems/UpdateSystem.cpp
)

target_include_directories(iron_maze PRIVATE src)
target_compile_options(iron_maze PRIVATE ${GAME_COMMON_WARNINGS})
target_link_libraries(iron_maze PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

enable_testing()

add_executable(gameplay_tests
    tests/test_progression.cpp
    src/data/GameData.cpp
    src/systems/MathUtils.cpp
    src/systems/Map.cpp
    src/systems/MazeGen.cpp
    src/systems/ParticleSystem.cpp
)
target_include_directories(gameplay_tests PRIVATE src)
target_compile_options(gameplay_tests PRIVATE ${GAME_COMMON_WARNINGS})
target_link_libraries(gameplay_tests PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
add_test(NAME gameplay_tests COMMAND gameplay_tests)
