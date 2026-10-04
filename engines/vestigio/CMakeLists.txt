# Compilación por targets: las políticas del proyecto no contaminan raylib.
cmake_minimum_required(VERSION 3.28)
project(Vestigio VERSION 0.1.0 LANGUAGES C CXX)
get_filename_component(ENGINE_REPO_ROOT "${PROJECT_SOURCE_DIR}/../.." ABSOLUTE)
include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(VG_BUILD_APPS "Construir aplicaciones con ventana" ON)
option(VG_ANALYZE "Ejecutar el analizador estático de GCC" OFF)
option(VG_SANITIZE "ASan y UBSan donde el compilador los soporte" OFF)

# Frontera pública inicial de VESTIGIO. Es deliberadamente header-only en F01:
# R01/R03 incorporarán runtime y distribución sin arrastrar tipos RetroForge.
add_library(vestigio_sdk_headers INTERFACE)
set_target_properties(vestigio_sdk_headers PROPERTIES EXPORT_NAME Headers)
target_include_directories(vestigio_sdk_headers INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${ENGINE_REPO_ROOT}/common/include>
    $<INSTALL_INTERFACE:include>)
add_library(Vestigio::Headers ALIAS vestigio_sdk_headers)

function(vestigio_target target)
    set_target_properties(${target} PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include" "${ENGINE_REPO_ROOT}/common/include")
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
        if(VG_ANALYZE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -fanalyzer)
        endif()
        if(VG_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC -fsanitize=address,undefined)
        endif()
        if(WIN32 AND NOT VG_SANITIZE)
            target_link_options(${target} PUBLIC -static -static-libgcc)
        endif()
    endif()
endfunction()

# Shared input/settings ABI used by both engines; no world, renderer or editor code.
add_library(engine_settings STATIC "${ENGINE_REPO_ROOT}/common/settings/settings.c")
set_target_properties(engine_settings PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(engine_settings PUBLIC
    $<BUILD_INTERFACE:${ENGINE_REPO_ROOT}/common/include>
    $<INSTALL_INTERFACE:include>)


# público permite que los consumidores de R01 enlacen el lifecycle ya real.
add_library(vestigio_runtime STATIC
    src/runtime/context.c
    src/runtime/camera.c
    src/runtime/game.c
    src/gamekit/controller.c
    src/gamekit/door.c
    src/gamekit/animation.c
    src/assets/asset_registry.c
    src/assets/static_model.c
    src/physics/spatial.c
    src/world/world.c
    src/world/spatial_world.c
    src/world/render_components.c
    src/world/transform.c)
set_target_properties(vestigio_runtime PROPERTIES EXPORT_NAME Runtime)
vestigio_target(vestigio_runtime)
set_property(TARGET vestigio_runtime PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(vestigio_runtime PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(vestigio_runtime PUBLIC vestigio_sdk_headers engine_settings)
if(NOT WIN32)
    target_link_libraries(vestigio_runtime PRIVATE m)
endif()
add_library(Vestigio::Runtime ALIAS vestigio_runtime)

# Audio ownership, buses and event deduplication can be tested without a device.
add_library(vestigio_audio STATIC src/audio/audio_core.c)
vestigio_target(vestigio_audio)
target_include_directories(vestigio_audio PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(vestigio_audio PUBLIC vestigio_sdk_headers)
if(NOT WIN32)
    target_link_libraries(vestigio_audio PRIVATE m)
endif()

# cgltf is vendored and compiled separately so third-party diagnostics do not
# weaken the warnings-as-errors policy applied to Vestigio's importer.
add_library(vestigio_cgltf STATIC src/assets/import/cgltf_impl.c)
set_target_properties(vestigio_cgltf PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
target_include_directories(vestigio_cgltf SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/cgltf")

add_library(vestigio_gltf_import STATIC
    src/assets/import/gltf_import.c
    src/assets/import/sha256.c)
vestigio_target(vestigio_gltf_import)
target_include_directories(vestigio_gltf_import PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(vestigio_gltf_import SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/cgltf")
target_link_libraries(vestigio_gltf_import PRIVATE vestigio_cgltf)
set_property(TARGET vestigio_gltf_import PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
if(NOT WIN32)
    target_link_libraries(vestigio_gltf_import PRIVATE m)
endif()
target_link_libraries(vestigio_runtime PRIVATE vestigio_gltf_import)
install(TARGETS vestigio_gltf_import vestigio_cgltf
    EXPORT VestigioTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT SDK)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/VestigioConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/VestigioConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Vestigio")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/VestigioConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)
install(TARGETS vestigio_runtime vestigio_sdk_headers engine_settings
    EXPORT VestigioTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT SDK
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "${ENGINE_REPO_ROOT}/common/include/engine_common"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT SDK)
install(DIRECTORY include/vestigio
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT SDK)
install(EXPORT VestigioTargets
    FILE VestigioTargets.cmake
    NAMESPACE Vestigio::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Vestigio"
    COMPONENT SDK)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VestigioConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VestigioConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Vestigio"
    COMPONENT SDK)

# Documentos de proyecto y nivel. Este target no depende del runtime ni de una
# plataforma grafica, de modo que los validadores se pueden ejecutar en CI CPU.
  add_library(vestigio_content STATIC
      src/content/json.c
      src/content/content.c
      src/content/room_recipe.c)
vestigio_target(vestigio_content)
target_include_directories(vestigio_content PUBLIC "${PROJECT_SOURCE_DIR}/src")
if(NOT WIN32)
    target_link_libraries(vestigio_content PRIVATE m)
endif()

add_library(vestigio_tool STATIC
    src/content/document.c
    src/content/document_runtime.c
    src/tooling/tool_api.c)
vestigio_target(vestigio_tool)
target_include_directories(vestigio_tool PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(vestigio_tool PUBLIC vestigio_content vestigio_runtime)
if(VG_BUILD_APPS)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    # Release 6.0, commit exacto. Hash del archivo comprimido descargado.
    set(raylib_source "${ENGINE_REPO_ROOT}/.deps/raylib-dbc56a87da87d973a9c5baa4e7438a9d20121d28")
    if(EXISTS "${raylib_source}/CMakeLists.txt")
        # bootstrap ya verificó el tarball antes de extraerlo. Usar SOURCE_DIR
        # evita duplicar ~90 MiB dentro de cada preset de compilación.
        FetchContent_Declare(raylib SOURCE_DIR "${raylib_source}")
    else()
        set(raylib_archive "${ENGINE_REPO_ROOT}/.deps/raylib-6.0.tar.gz")
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
    add_library(vestigio_gpu_raylib STATIC src/render/gpu_raylib/gpu_renderer.c)
    vestigio_target(vestigio_gpu_raylib)
    target_include_directories(vestigio_gpu_raylib PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_include_directories(vestigio_gpu_raylib SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    target_link_libraries(vestigio_gpu_raylib PUBLIC vestigio_sdk_headers PRIVATE raylib vestigio_runtime)
    add_library(vestigio_audio_raylib STATIC src/audio/raylib_audio.c)
    vestigio_target(vestigio_audio_raylib)
    target_include_directories(vestigio_audio_raylib PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_include_directories(vestigio_audio_raylib SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    target_link_libraries(vestigio_audio_raylib PUBLIC vestigio_audio PRIVATE raylib)
    if(WIN32)
        add_library(vestigio_gpu_host SHARED src/platform/gpu_host.c src/platform/win32_embed.c
            src/audio/atrium_audio.c src/gamekit/atrium_animation.c)
        vestigio_target(vestigio_gpu_host)
        target_compile_definitions(vestigio_gpu_host PRIVATE VG_GPU_HOST_BUILD)
        target_include_directories(vestigio_gpu_host PRIVATE "${PROJECT_SOURCE_DIR}/src")
        target_include_directories(vestigio_gpu_host SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
        target_link_libraries(vestigio_gpu_host PRIVATE vestigio_gpu_raylib vestigio_tool
            vestigio_audio_raylib raylib comctl32)
        set_target_properties(vestigio_gpu_host PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS NO)
        install(TARGETS vestigio_gpu_host RUNTIME DESTINATION . COMPONENT Runtime)
    endif()
    if(BUILD_TESTING)
        add_executable(vestigio_audio_device_test tests/audio/raylib_audio_test.c)
        vestigio_target(vestigio_audio_device_test)
        target_link_libraries(vestigio_audio_device_test PRIVATE vestigio_audio_raylib raylib)
        add_test(NAME vestigio_audio_device COMMAND vestigio_audio_device_test
            "${PROJECT_SOURCE_DIR}/assets/demo/audio/atrium-ambience.wav"
            "${PROJECT_SOURCE_DIR}/assets/demo/audio/door-move.wav")
        set_tests_properties(vestigio_audio_device PROPERTIES LABELS "audio;device;app")
        if(WIN32)
            add_executable(vestigio_gpu_host_room_test tests/platform/gpu_host_room_test.c)
            vestigio_target(vestigio_gpu_host_room_test)
            target_include_directories(vestigio_gpu_host_room_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
            target_link_libraries(vestigio_gpu_host_room_test PRIVATE vestigio_gpu_host)
            add_test(NAME vestigio_gpu_host_room COMMAND vestigio_gpu_host_room_test
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf"
                "${CMAKE_BINARY_DIR}/w09-room-copy.level.json")
            set_tests_properties(vestigio_gpu_host_room PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                LABELS "window;gpu;studio;document;spatial;app")
            add_executable(vestigio_gpu_host_e02_test tests/platform/gpu_host_e02_test.c)
            vestigio_target(vestigio_gpu_host_e02_test)
            target_include_directories(vestigio_gpu_host_e02_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
            target_link_libraries(vestigio_gpu_host_e02_test PRIVATE vestigio_gpu_host vestigio_tool)
            add_test(NAME vestigio_gpu_host_e02 COMMAND vestigio_gpu_host_e02_test
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf"
                "${CMAKE_BINARY_DIR}/e02-copy.level.json")
            set_tests_properties(vestigio_gpu_host_e02 PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                LABELS "window;gpu;studio;document;app")
            add_executable(vestigio_gpu_host_e03_test tests/platform/gpu_host_e03_test.c)
            vestigio_target(vestigio_gpu_host_e03_test)
            target_include_directories(vestigio_gpu_host_e03_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
            target_link_libraries(vestigio_gpu_host_e03_test PRIVATE vestigio_gpu_host vestigio_tool)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/e03-host-evidence")
            add_test(NAME vestigio_gpu_host_e03 COMMAND vestigio_gpu_host_e03_test
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf"
                "${PROJECT_SOURCE_DIR}/tests/assets/fixtures/static_triangle.glb"
                "${CMAKE_BINARY_DIR}/e03-host-evidence")
            set_tests_properties(vestigio_gpu_host_e03 PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                LABELS "window;gpu;studio;asset;document;app")
            add_executable(vestigio_gpu_host_e04_test tests/platform/gpu_host_e04_test.c)
            vestigio_target(vestigio_gpu_host_e04_test)
            target_include_directories(vestigio_gpu_host_e04_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
            target_link_libraries(vestigio_gpu_host_e04_test PRIVATE vestigio_gpu_host)
            add_test(NAME vestigio_gpu_host_e04 COMMAND vestigio_gpu_host_e04_test
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf"
                "${CMAKE_BINARY_DIR}/e04-room-copy.level.json")
            set_tests_properties(vestigio_gpu_host_e04 PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/bin"
                LABELS "window;gpu;studio;document;spatial;app")
            add_test(NAME vestigio_studio_3d COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e01 "${CMAKE_BINARY_DIR}/studio-3d-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf")
            set_tests_properties(vestigio_studio_3d PROPERTIES
                LABELS "window;gpu;studio;document;app")
            add_test(NAME vestigio_studio_editing COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e04 "${CMAKE_BINARY_DIR}/studio-editing-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_editing PROPERTIES
                LABELS "window;gpu;studio;document;app")
            add_test(NAME vestigio_studio_door COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e05 "${CMAKE_BINARY_DIR}/studio-door-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.gltf")
            set_tests_properties(vestigio_studio_door PROPERTIES
                LABELS "window;gpu;studio;document;gamekit;app")
            add_test(NAME vestigio_studio_visual COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e06 "${CMAKE_BINARY_DIR}/studio-visual-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_visual PROPERTIES
                FIXTURES_SETUP vestigio_visual_settings
                LABELS "window;gpu;studio;document;visual;app")
            add_test(NAME vestigio_studio_audio COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e07 "${CMAKE_BINARY_DIR}/studio-audio-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_audio PROPERTIES
                LABELS "window;gpu;studio;audio;settings;app")
            add_test(NAME vestigio_studio_room COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --w09 "${CMAKE_BINARY_DIR}/studio-room-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_room PROPERTIES
                LABELS "window;gpu;studio;document;spatial;app")
            add_test(NAME vestigio_studio_e02 COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e02 "${CMAKE_BINARY_DIR}/studio-e02-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_e02 PROPERTIES
                LABELS "window;gpu;studio;document;app")
            add_test(NAME vestigio_studio_e03 COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e03 "${CMAKE_BINARY_DIR}/studio-e03-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/tests/assets/fixtures/static_triangle.glb")
            set_tests_properties(vestigio_studio_e03 PROPERTIES
                LABELS "window;gpu;studio;asset;document;app")
            add_test(NAME vestigio_studio_e04_room COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --e04-room-recipe "${CMAKE_BINARY_DIR}/studio-e04-evidence"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json")
            set_tests_properties(vestigio_studio_e04_room PROPERTIES
                LABELS "window;gpu;studio;document;spatial;app")
            add_test(NAME vestigio_studio_ux01 COMMAND dotnet
                "${CMAKE_BINARY_DIR}/bin/Vestigio.Studio.Tests.dll"
                --ux01 "${CMAKE_BINARY_DIR}/studio-ux01-evidence"
                "${PROJECT_SOURCE_DIR}/tests/assets/fixtures/static_triangle.glb")
            set_tests_properties(vestigio_studio_ux01 PROPERTIES
                LABELS "window;gpu;studio;document;asset;app;ux")
        endif()
        add_executable(vestigio_gpu_test tests/gpu/gpu_renderer_test.c)
        vestigio_target(vestigio_gpu_test)
        target_link_libraries(vestigio_gpu_test PRIVATE vestigio_gpu_raylib raylib)
        target_include_directories(vestigio_gpu_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
        add_test(NAME vestigio_gpu_render COMMAND vestigio_gpu_test)
        set_tests_properties(vestigio_gpu_render PROPERTIES LABELS "window;gpu")
        add_executable(vestigio_gpu_scene_test tests/gpu/gpu_scene_test.c)
        vestigio_target(vestigio_gpu_scene_test)
        target_link_libraries(vestigio_gpu_scene_test PRIVATE
            vestigio_gpu_raylib vestigio_runtime vestigio_gltf_import raylib)
        target_include_directories(vestigio_gpu_scene_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
        add_test(NAME vestigio_gpu_scene COMMAND vestigio_gpu_scene_test)
        set_tests_properties(vestigio_gpu_scene PROPERTIES
            LABELS "window;gpu;asset;sdk"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()
    add_executable(vestigio_player src/player/demo_main.c src/player/demo_3d.c
        src/player/demo_scene.c src/audio/atrium_audio.c src/gamekit/atrium_animation.c)
    vestigio_target(vestigio_player)
    target_include_directories(vestigio_player PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_include_directories(vestigio_player SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    target_link_libraries(vestigio_player PRIVATE vestigio_gpu_raylib vestigio_tool
        vestigio_audio_raylib)
    if(WIN32)
        target_link_libraries(vestigio_player PRIVATE shell32)
    endif()
    set_target_properties(vestigio_player PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    install(TARGETS vestigio_player RUNTIME DESTINATION . COMPONENT Runtime)
    add_custom_target(vestigio_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/bin/assets")
    add_dependencies(vestigio_player vestigio_assets)
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(NAME vestigio_player_e03_import COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/player/e03_imported_level.py"
                "$<TARGET_FILE:vestigio_player>"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${PROJECT_SOURCE_DIR}/tests/assets/fixtures/static_triangle.glb"
                "${CMAKE_BINARY_DIR}/e03-player-evidence")
            set_tests_properties(vestigio_player_e03_import PROPERTIES
                LABELS "window;gpu;asset;document;app")
            add_test(NAME vestigio_player_e04_room COMMAND "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/tests/player/e04_room_level.py"
                "$<TARGET_FILE:vestigio_player>"
                "${PROJECT_SOURCE_DIR}/assets/demo/atrium.level.json"
                "${CMAKE_BINARY_DIR}/e04-player-evidence")
            set_tests_properties(vestigio_player_e04_room PROPERTIES
                LABELS "window;gpu;document;spatial;app")
        else()
            message(WARNING "Python 3 is required to run Player E03/E04 integration tests; the application itself does not require Python")
        endif()
        add_test(NAME vestigio_player_demo COMMAND vestigio_player --smoke 8)
        set_tests_properties(vestigio_player_demo PROPERTIES LABELS "window;gpu;sdk;app")
        add_test(NAME vestigio_player_collision COMMAND vestigio_player --smoke 240)
        set_tests_properties(vestigio_player_collision PROPERTIES
            LABELS "window;gpu;spatial;gamekit;app")
        add_test(NAME vestigio_player_door COMMAND vestigio_player --smoke 180 --smoke-door)
        set_tests_properties(vestigio_player_door PROPERTIES
            LABELS "window;gpu;spatial;gamekit;document;app")
        add_test(NAME vestigio_player_visual COMMAND vestigio_player
            --smoke 8 --visual retro --settings "${CMAKE_BINARY_DIR}/w06-player.settings")
        set_tests_properties(vestigio_player_visual PROPERTIES
            LABELS "window;gpu;document;visual;app")
        if(WIN32)
            add_test(NAME vestigio_player_visual_persisted COMMAND vestigio_player
                --smoke 8 --settings
                "${CMAKE_BINARY_DIR}/studio-visual-evidence/preferencias-área/e06-visual.settings")
            set_tests_properties(vestigio_player_visual_persisted PROPERTIES
                FIXTURES_REQUIRED vestigio_visual_settings
                PASS_REGULAR_EXPRESSION "visual=retro lights=2 fog=linear"
                LABELS "window;gpu;document;visual;settings;app")
        endif()
        add_test(NAME vestigio_player_audio COMMAND vestigio_player
            --smoke 180 --smoke-door --smoke-audio
            --settings "${CMAKE_BINARY_DIR}/w07-player-device.settings")
        set_tests_properties(vestigio_player_audio PROPERTIES
            LABELS "window;gpu;audio;gamekit;app")
        add_test(NAME vestigio_player_audio_save COMMAND vestigio_player
            --smoke 8 --no-audio --save-audio
            --settings "${CMAKE_BINARY_DIR}/w07-áudio.settings"
            --volume-master 0.55 --volume-music 0.4
            --volume-sfx 0.7 --volume-ambience 0.2)
        set_tests_properties(vestigio_player_audio_save PROPERTIES
            FIXTURES_SETUP vestigio_audio_settings
            LABELS "window;gpu;audio;settings;app")
        add_test(NAME vestigio_player_audio_load COMMAND vestigio_player
            --smoke 8 --no-audio
            --settings "${CMAKE_BINARY_DIR}/w07-áudio.settings")
        set_tests_properties(vestigio_player_audio_load PROPERTIES
            FIXTURES_REQUIRED vestigio_audio_settings
            PASS_REGULAR_EXPRESSION
                "audio gains master=0.55 music=0.40 sfx=0.70 ambience=0.20"
            LABELS "window;gpu;audio;settings;app")
    endif()
    install(DIRECTORY assets DESTINATION . COMPONENT Runtime)
    install(FILES "${ENGINE_REPO_ROOT}/README.md" "${ENGINE_REPO_ROOT}/THIRD_PARTY.md"
        DESTINATION . COMPONENT Runtime)
    install(FILES "${raylib_SOURCE_DIR}/LICENSE" DESTINATION licenses COMPONENT Runtime RENAME raylib.txt)
    install(FILES "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md"
        DESTINATION licenses COMPONENT Runtime RENAME glfw.txt)
    install(DIRECTORY "${raylib_SOURCE_DIR}/src/external/" DESTINATION licenses/raylib-external
        COMPONENT Runtime FILES_MATCHING PATTERN "*.h" PATTERN "*.c" PATTERN "glfw" EXCLUDE)
endif()
if(BUILD_TESTING)
    add_executable(vestigio_audio_core_test tests/audio/audio_core_test.c)
    vestigio_target(vestigio_audio_core_test)
    target_link_libraries(vestigio_audio_core_test PRIVATE vestigio_audio)
    add_test(NAME vestigio_audio_core COMMAND vestigio_audio_core_test)
    set_tests_properties(vestigio_audio_core PROPERTIES LABELS "audio;core")

    add_executable(vestigio_spatial_test tests/spatial/spatial_test.c)
    vestigio_target(vestigio_spatial_test)
    target_link_libraries(vestigio_spatial_test PRIVATE vestigio_runtime)
    add_test(NAME vestigio_spatial COMMAND vestigio_spatial_test)

    add_executable(vestigio_content_test tests/content/content_test.c)
    vestigio_target(vestigio_content_test)
    target_link_libraries(vestigio_content_test PRIVATE vestigio_content)
    target_compile_definitions(vestigio_content_test PRIVATE
        VG_CONTENT_FIXTURES="${PROJECT_SOURCE_DIR}/tests/content/fixtures")
    add_test(NAME vestigio_content COMMAND vestigio_content_test)

    add_executable(vestigio_room_recipe_test tests/content/room_recipe_test.c)
    vestigio_target(vestigio_room_recipe_test)
    target_include_directories(vestigio_room_recipe_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(vestigio_room_recipe_test PRIVATE vestigio_content)
    add_test(NAME vestigio_room_recipe COMMAND vestigio_room_recipe_test)

    add_executable(vestigio_document_test tests/content/document_test.c)
    vestigio_target(vestigio_document_test)
    target_link_libraries(vestigio_document_test PRIVATE vestigio_tool)
    target_compile_definitions(vestigio_document_test PRIVATE
        VG_CONTENT_FIXTURES="${PROJECT_SOURCE_DIR}/tests/content/fixtures"
        VG_CONTENT_TEST_DIR="${CMAKE_CURRENT_BINARY_DIR}/document-test")
    add_test(NAME vestigio_document COMMAND vestigio_document_test)

    add_executable(vestigio_document_runtime_test tests/content/document_runtime_test.c)
    vestigio_target(vestigio_document_runtime_test)
    target_link_libraries(vestigio_document_runtime_test PRIVATE vestigio_tool)
    target_compile_definitions(vestigio_document_runtime_test PRIVATE
        VG_CONTENT_FIXTURES="${PROJECT_SOURCE_DIR}/tests/content/fixtures")
    add_test(NAME vestigio_document_runtime COMMAND vestigio_document_runtime_test)

    add_executable(vestigio_runtime_test tests/runtime/runtime_test.c)
    vestigio_target(vestigio_runtime_test)
    target_link_libraries(vestigio_runtime_test PRIVATE vestigio_runtime)
    target_include_directories(vestigio_runtime_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME vestigio_runtime COMMAND vestigio_runtime_test)

    add_executable(vestigio_controller_test tests/gamekit/controller_test.c)
    vestigio_target(vestigio_controller_test)
    target_link_libraries(vestigio_controller_test PRIVATE vestigio_runtime)
    add_test(NAME vestigio_controller COMMAND vestigio_controller_test)
    add_executable(vestigio_door_test tests/gamekit/door_test.c)
    vestigio_target(vestigio_door_test)
    target_link_libraries(vestigio_door_test PRIVATE vestigio_runtime)
    add_test(NAME vestigio_door COMMAND vestigio_door_test)
    set_tests_properties(vestigio_door PROPERTIES LABELS "gamekit;spatial")
    add_executable(vestigio_animation_test tests/gamekit/animation_test.c)
    vestigio_target(vestigio_animation_test)
    target_link_libraries(vestigio_animation_test PRIVATE vestigio_runtime)
    add_test(NAME vestigio_animation COMMAND vestigio_animation_test)
    set_tests_properties(vestigio_animation PROPERTIES LABELS "gamekit;animation")
    set_tests_properties(vestigio_controller PROPERTIES LABELS "spatial;gamekit;sdk")

    add_executable(vestigio_runtime_cpp_test tests/runtime/runtime_link_cpp_test.cpp)
    target_link_libraries(vestigio_runtime_cpp_test PRIVATE vestigio_runtime)
    set_target_properties(vestigio_runtime_cpp_test PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME vestigio_runtime_cpp11 COMMAND vestigio_runtime_cpp_test)

    add_executable(vestigio_asset_test tests/assets/asset_registry_test.c)
    vestigio_target(vestigio_asset_test)
    target_link_libraries(vestigio_asset_test PRIVATE vestigio_runtime)
    target_include_directories(vestigio_asset_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME vestigio_assets COMMAND vestigio_asset_test)

    add_executable(vestigio_gltf_import_test tests/assets/gltf_import_test.c)
    vestigio_target(vestigio_gltf_import_test)
    if(VG_ANALYZE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # CHECK exits the short-lived test process immediately; GCC's analyzer
        # otherwise reports those assertion-only paths as leaks/null accesses.
        target_compile_options(vestigio_gltf_import_test PRIVATE
            -Wno-analyzer-malloc-leak -Wno-analyzer-null-dereference)
    endif()
    target_include_directories(vestigio_gltf_import_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(vestigio_gltf_import_test PRIVATE vestigio_gltf_import)
    add_test(NAME vestigio_gltf_import
        COMMAND vestigio_gltf_import_test "tests/assets/fixtures")
    set_tests_properties(vestigio_gltf_import PROPERTIES
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

    add_executable(vestigio_game_test tests/sdk/game_test.c)
    vestigio_target(vestigio_game_test)
    target_link_libraries(vestigio_game_test PRIVATE vestigio_runtime)
    add_test(NAME vestigio_game_lifecycle COMMAND vestigio_game_test)

    add_executable(vestigio_runtime_c_test tests/runtime/runtime_link_c_test.c)
    target_link_libraries(vestigio_runtime_c_test PRIVATE vestigio_runtime)
    set_target_properties(vestigio_runtime_c_test PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME vestigio_runtime_c11 COMMAND vestigio_runtime_c_test)

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

    add_test(NAME vestigio_sdk_install
        COMMAND "${CMAKE_COMMAND}"
            "-DVG_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DVG_CONSUMER_SOURCE=${PROJECT_SOURCE_DIR}/tests/sdk/external-consumer"
            "-DVG_GENERATOR=${CMAKE_GENERATOR}"
            "-DVG_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DVG_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DVG_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DVG_EXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
            -P "${PROJECT_SOURCE_DIR}/tests/sdk/verify_install.cmake")

endif()
