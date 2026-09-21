# Compilación por targets: las políticas del proyecto no contaminan raylib.
cmake_minimum_required(VERSION 3.28)
project(RetroForge VERSION 0.1.0 LANGUAGES C CXX)
include(CTest)
option(RETRO_BUILD_APPS "Construir aplicaciones con ventana" ON)
option(RETRO_ANALYZE "Ejecutar el analizador estático de GCC" OFF)
option(RETRO_SANITIZE "ASan y UBSan donde el compilador los soporte" OFF)

# Frontera pública inicial de VESTIGIO. Es deliberadamente header-only en F01:
# R01/R03 incorporarán runtime y distribución sin arrastrar tipos RetroForge.
add_library(vestigio_sdk_headers INTERFACE)
target_include_directories(vestigio_sdk_headers INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
add_library(Vestigio::Headers ALIAS vestigio_sdk_headers)

function(retro_target target)
    set_target_properties(${target} PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Algunas tablas de CMake siguen emitiendo el alias histórico c2x.
        # Seleccionamos explícitamente el nombre del estándar publicado.
        target_compile_options(${target} PRIVATE -std=c23)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 -Wundef)
        # -Og mantiene una experiencia de depuración útil (variables y flujo
        # reconocibles) sin obligar al renderer CPU a ejecutar cada operación
        # elemental sin optimizar. Release conserva sus flags propios de CMake.
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:-Og>)
        if(RETRO_ANALYZE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -fanalyzer)
        endif()
        if(RETRO_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC -fsanitize=address,undefined)
        endif()
        if(WIN32 AND NOT RETRO_SANITIZE)
            target_link_options(${target} PUBLIC -static -static-libgcc)
        endif()
    endif()
endfunction()

add_library(retro_core STATIC src/engine/math.c src/engine/map.c src/engine/world.c
    src/engine/render.c src/engine/canvas.c src/engine/input.c)
retro_target(retro_core)
# Studio inicia el Player del mismo preset. El núcleo gráfico necesita velocidad
# representativa incluso en Debug; gameplay, editor y aplicaciones permanecen
# en -Og para inspeccionar su flujo con el depurador.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(retro_core PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()
if(NOT WIN32)
    target_link_libraries(retro_core PUBLIC m)
endif()

# Reglas reutilizables y formatos de proyecto. Sigue siendo una biblioteca sin
# ventana: editor, juegos y pruebas ejecutan exactamente la misma simulación.
add_library(retro_gameplay STATIC src/gameplay/gameplay.c src/gameplay/interaction.c
    src/gameplay/project.c src/gameplay/transaction.c src/gameplay/character_art.c)
retro_target(retro_gameplay)
target_link_libraries(retro_gameplay PUBLIC retro_core)

# Fachada ABI estable para RetroForge Studio. La interfaz WPF nunca accede a
# las estructuras internas del motor ni comparte asignaciones entre C y C#.
add_library(retro_editor SHARED src/editor/editor.c)
retro_target(retro_editor)
target_compile_definitions(retro_editor PRIVATE RETRO_EDITOR_BUILD)
target_link_libraries(retro_editor PRIVATE retro_gameplay)
set_target_properties(retro_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PREFIX ""
    WINDOWS_EXPORT_ALL_SYMBOLS YES)

add_library(fps_game STATIC src/fps/game.c src/fps/art.c src/fps/view.c)
retro_target(fps_game)
target_link_libraries(fps_game PUBLIC retro_core retro_gameplay)
target_include_directories(fps_game PUBLIC "${PROJECT_SOURCE_DIR}/src/fps")

if(RETRO_BUILD_APPS)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    # Release 6.0, commit exacto. Hash del archivo comprimido descargado.
    set(raylib_source "${PROJECT_SOURCE_DIR}/.deps/raylib-dbc56a87da87d973a9c5baa4e7438a9d20121d28")
    if(EXISTS "${raylib_source}/CMakeLists.txt")
        # bootstrap ya verificó el tarball antes de extraerlo. Usar SOURCE_DIR
        # evita duplicar ~90 MiB dentro de cada preset de compilación.
        FetchContent_Declare(raylib SOURCE_DIR "${raylib_source}")
    else()
        set(raylib_archive "${PROJECT_SOURCE_DIR}/.deps/raylib-6.0.tar.gz")
        if(NOT EXISTS "${raylib_archive}")
            set(raylib_archive "https://github.com/raysan5/raylib/archive/dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz")
        endif()
        FetchContent_Declare(raylib URL "${raylib_archive}"
            URL_HASH SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_MakeAvailable(raylib)
    # raygui es header-only. La implementación vive en un target de tercero para
    # que sus avisos no relajen -Werror en nuestro Studio.
    add_library(retro_platform STATIC src/platform/raylib_platform.c)
    retro_target(retro_platform)
    target_link_libraries(retro_platform PUBLIC retro_core PRIVATE raylib)
    target_include_directories(retro_platform SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    # Backend GPU nuevo, aislado del renderer software y de la API pública.
    add_library(vestigio_gpu_raylib STATIC src/render/gpu_raylib/gpu_renderer.c)
    retro_target(vestigio_gpu_raylib)
    target_include_directories(vestigio_gpu_raylib PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_include_directories(vestigio_gpu_raylib SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    target_link_libraries(vestigio_gpu_raylib PUBLIC vestigio_sdk_headers PRIVATE raylib)
    if(WIN32)
        add_library(vestigio_gpu_host SHARED src/platform/gpu_host.c src/platform/win32_embed.c)
        retro_target(vestigio_gpu_host)
        target_compile_definitions(vestigio_gpu_host PRIVATE VG_GPU_HOST_BUILD)
        target_include_directories(vestigio_gpu_host PRIVATE "${PROJECT_SOURCE_DIR}/src")
        target_include_directories(vestigio_gpu_host SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
        target_link_libraries(vestigio_gpu_host PRIVATE vestigio_gpu_raylib raylib)
        set_target_properties(vestigio_gpu_host PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS NO)
    endif()
    # La DLL comparte simulación, presentación CPU y recursos entre WPF y Player.
    add_library(retro_session SHARED src/session/session.c)
    retro_target(retro_session)
    target_link_libraries(retro_session PRIVATE retro_gameplay retro_platform)
    set_target_properties(retro_session PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS YES)
    target_link_libraries(retro_editor PRIVATE retro_session)
    target_compile_definitions(retro_editor PRIVATE RETRO_EDITOR_SESSIONS)
    install(TARGETS retro_session RUNTIME DESTINATION . COMPONENT Runtime)
    if(BUILD_TESTING)
        add_executable(retro_session_test tests/session_test.c)
        retro_target(retro_session_test)
        target_compile_definitions(retro_session_test PRIVATE RETRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        target_link_libraries(retro_session_test PRIVATE retro_editor retro_session)
        set_target_properties(retro_session_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_test(NAME retro_shared_session COMMAND retro_session_test)
        if(WIN32)
            add_test(NAME retro_studio_authoring COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/RetroForge.Studio.Tests.dll"
                "${CMAKE_BINARY_DIR}/studio-evidence"
                "${PROJECT_SOURCE_DIR}/assets/studio/haunted.retro")
        endif()
        add_executable(retro_platform_test tests/platform_test.c)
        retro_target(retro_platform_test)
        target_link_libraries(retro_platform_test PRIVATE retro_platform)
        add_test(NAME retro_window COMMAND retro_platform_test)
        set_tests_properties(retro_window PROPERTIES LABELS "window")
        add_executable(vestigio_gpu_test tests/gpu/gpu_renderer_test.c)
        retro_target(vestigio_gpu_test)
        target_link_libraries(vestigio_gpu_test PRIVATE vestigio_gpu_raylib retro_platform)
        target_include_directories(vestigio_gpu_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
        add_test(NAME vestigio_gpu_render COMMAND vestigio_gpu_test)
        set_tests_properties(vestigio_gpu_render PROPERTIES LABELS "window;gpu")
    endif()
    add_executable(retro_fps src/fps/main.c)
    retro_target(retro_fps)
    target_link_libraries(retro_fps PRIVATE fps_game retro_platform)
    add_executable(retro_lab src/lab/main.c)
    retro_target(retro_lab)
    target_link_libraries(retro_lab PRIVATE retro_platform)
    add_executable(retro_player src/player/main.c)
    retro_target(retro_player)
    target_link_libraries(retro_player PRIVATE retro_session retro_gameplay retro_platform)
    foreach(app retro_fps retro_lab retro_player)
        set_target_properties(${app} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        install(TARGETS ${app} RUNTIME DESTINATION . COMPONENT Runtime)
    endforeach()
    add_custom_target(retro_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/bin/assets")
    add_dependencies(retro_fps retro_assets)
    add_dependencies(retro_lab retro_assets)
    add_dependencies(retro_player retro_assets)
    install(TARGETS retro_editor RUNTIME DESTINATION . COMPONENT Runtime)
    install(DIRECTORY assets DESTINATION . COMPONENT Runtime)
    install(FILES README.md THIRD_PARTY.md DESTINATION . COMPONENT Runtime)
    install(FILES "${raylib_SOURCE_DIR}/LICENSE" DESTINATION licenses COMPONENT Runtime RENAME raylib.txt)
    install(FILES "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md" DESTINATION licenses COMPONENT Runtime RENAME glfw.txt)
    # Se conservan los avisos en sus fuentes originales, sin extraer fragmentos
    # ambiguos de licencias embebidas (stb, miniaudio, cargadores, etc.).
    install(DIRECTORY "${raylib_SOURCE_DIR}/src/external/" DESTINATION licenses/raylib-external
        COMPONENT Runtime FILES_MATCHING PATTERN "*.h" PATTERN "*.c" PATTERN "glfw" EXCLUDE)
    if(WIN32)
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/.tools/msys64/ucrt64/share/licenses/gcc-libs"
            "${PROJECT_SOURCE_DIR}/.tools/msys64/ucrt64/share/licenses/crt"
            DESTINATION licenses COMPONENT Runtime OPTIONAL)
    endif()
    install(DIRECTORY docs DESTINATION . COMPONENT Runtime)
    install(FILES tools/export-project.ps1 DESTINATION tools COMPONENT Runtime)
endif()

if(BUILD_TESTING)
    add_executable(vestigio_header_c_test tests/sdk/header_c_test.c)
    target_link_libraries(vestigio_header_c_test PRIVATE vestigio_sdk_headers)
    set_target_properties(vestigio_header_c_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME vestigio_header_c11 COMMAND vestigio_header_c_test)

    add_executable(vestigio_header_cpp_test tests/sdk/header_cpp_test.cpp)
    target_link_libraries(vestigio_header_cpp_test PRIVATE vestigio_sdk_headers)
    set_target_properties(vestigio_header_cpp_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME vestigio_header_cpp11 COMMAND vestigio_header_cpp_test)

    add_executable(vestigio_render_contract_test tests/sdk/render_contract_test.c)
    target_link_libraries(vestigio_render_contract_test PRIVATE vestigio_sdk_headers)
    target_include_directories(vestigio_render_contract_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    set_target_properties(vestigio_render_contract_test PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME vestigio_render_contract COMMAND vestigio_render_contract_test)

    add_executable(retro_tests tests/test_main.c)
    retro_target(retro_tests)
    target_link_libraries(retro_tests PRIVATE fps_game retro_editor)
    target_compile_definitions(retro_tests PRIVATE RETRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_compile_definitions(retro_tests PRIVATE RETRO_TEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    set_target_properties(retro_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME retro_contracts COMMAND retro_tests)
endif()
