if(NOT DEFINED VG_BUILD_DIR OR NOT DEFINED VG_CONSUMER_SOURCE OR
   NOT DEFINED VG_GENERATOR OR NOT DEFINED VG_C_COMPILER OR
   NOT DEFINED VG_MAKE_PROGRAM)
    message(FATAL_ERROR "Missing SDK verification arguments")
endif()

string(SHA256 vg_test_id "${VG_BUILD_DIR}")
string(SUBSTRING "${vg_test_id}" 0 12 vg_test_id)
set(vg_root "$ENV{TEMP}/Vestigio SDK external ${vg_test_id}")
set(vg_prefix "${vg_root}/clean install")
set(vg_source "${vg_root}/consumer source")
set(vg_build "${vg_root}/consumer build")
set(vg_cwd "${vg_root}/arbitrary cwd")
file(REMOVE_RECURSE "${vg_root}")
file(MAKE_DIRECTORY "${vg_root}" "${vg_source}" "${vg_cwd}")
file(COPY "${VG_CONSUMER_SOURCE}/" DESTINATION "${vg_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${VG_BUILD_DIR}" --prefix "${vg_prefix}"
            --config "${VG_BUILD_TYPE}" --component SDK
    RESULT_VARIABLE vg_result
    OUTPUT_VARIABLE vg_stdout
    ERROR_VARIABLE vg_stderr)
if(NOT vg_result EQUAL 0)
    message(FATAL_ERROR "SDK install failed (${vg_result})\n${vg_stdout}\n${vg_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${vg_source}" -B "${vg_build}" -G "${VG_GENERATOR}"
            "-DCMAKE_C_COMPILER=${VG_C_COMPILER}"
            "-DCMAKE_MAKE_PROGRAM=${VG_MAKE_PROGRAM}"
            "-DCMAKE_BUILD_TYPE=${VG_BUILD_TYPE}"
            "-DCMAKE_PREFIX_PATH=${vg_prefix}"
            -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
    RESULT_VARIABLE vg_result
    OUTPUT_VARIABLE vg_stdout
    ERROR_VARIABLE vg_stderr)
if(NOT vg_result EQUAL 0)
    message(FATAL_ERROR "External SDK configure failed (${vg_result})\n${vg_stdout}\n${vg_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${vg_build}" --config "${VG_BUILD_TYPE}"
    RESULT_VARIABLE vg_result
    OUTPUT_VARIABLE vg_stdout
    ERROR_VARIABLE vg_stderr)
if(NOT vg_result EQUAL 0)
    message(FATAL_ERROR "External SDK build failed (${vg_result})\n${vg_stdout}\n${vg_stderr}")
endif()

execute_process(
    COMMAND "${vg_build}/vestigio_external_consumer${VG_EXE_SUFFIX}"
    WORKING_DIRECTORY "${vg_cwd}"
    RESULT_VARIABLE vg_result
    OUTPUT_VARIABLE vg_stdout
    ERROR_VARIABLE vg_stderr)
if(NOT vg_result EQUAL 0)
    message(FATAL_ERROR "External SDK run failed (${vg_result})\n${vg_stdout}\n${vg_stderr}")
endif()

message(STATUS "Installed SDK configured, built and ran from ${vg_root}")
