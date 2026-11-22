cmake_minimum_required(VERSION 3.16)
project(ps4_neural_engine_telemetry LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
add_library(pne_telemetry
    src/telemetry.cpp
    src/telemetry_bridge.cpp
)
target_include_directories(pne_telemetry PUBLIC include)
target_link_libraries(pne_telemetry PUBLIC Threads::Threads)
add_executable(test_telemetry tests/test_telemetry.cpp)
target_link_libraries(test_telemetry PRIVATE pne_telemetry)
enable_testing()
add_test(NAME telemetry_writer COMMAND test_telemetry)

add_executable(test_telemetry_bridge tests/test_telemetry_bridge.cpp)
target_link_libraries(test_telemetry_bridge PRIVATE pne_telemetry)
add_test(NAME telemetry_bridge COMMAND test_telemetry_bridge)

add_library(pne_gcn8_contract
    platform/ps4/gcn8/host/compute_contract.cpp
)
target_include_directories(pne_gcn8_contract PUBLIC platform/ps4/gcn8/host)

add_executable(test_gcn8_contract tests/test_gcn8_contract.cpp)
target_link_libraries(test_gcn8_contract PRIVATE pne_gcn8_contract)
add_test(NAME gcn8_compute_contract COMMAND test_gcn8_contract)

add_library(pne_gcn8_resource_table
    platform/ps4/gcn8/host/resource_table_contract.cpp
)
target_include_directories(pne_gcn8_resource_table PUBLIC platform/ps4/gcn8/host)

add_executable(test_gcn8_resource_table tests/test_gcn8_resource_table.cpp)
target_link_libraries(test_gcn8_resource_table PRIVATE pne_gcn8_resource_table)
add_test(NAME gcn8_resource_table_contract COMMAND test_gcn8_resource_table)

add_library(pne_gcn8_shader_binary
    platform/ps4/gcn8/host/shader_binary_contract.cpp
)
target_include_directories(pne_gcn8_shader_binary PUBLIC platform/ps4/gcn8/host)

add_executable(test_gcn8_shader_binary tests/test_gcn8_shader_binary.cpp)
target_link_libraries(test_gcn8_shader_binary PRIVATE pne_gcn8_shader_binary)
add_test(NAME gcn8_shader_binary_contract COMMAND test_gcn8_shader_binary)


add_library(pne_gcn8_vsharp_codec
    platform/ps4/gcn8/host/vsharp_codec.cpp
)
target_include_directories(pne_gcn8_vsharp_codec PUBLIC platform/ps4/gcn8/host)
add_executable(test_gcn8_vsharp_codec tests/test_gcn8_vsharp_codec.cpp)
target_link_libraries(test_gcn8_vsharp_codec PRIVATE pne_gcn8_vsharp_codec)
add_test(NAME gcn8_vsharp_codec COMMAND test_gcn8_vsharp_codec)

add_library(pne_gcn8_arena
    platform/ps4/gcn8/host/arena_contract.cpp
)
target_include_directories(pne_gcn8_arena PUBLIC platform/ps4/gcn8/host)
add_executable(test_gcn8_arena_contract tests/test_gcn8_arena_contract.cpp)
target_link_libraries(test_gcn8_arena_contract PRIVATE pne_gcn8_arena)
add_test(NAME gcn8_arena_contract COMMAND test_gcn8_arena_contract)

add_executable(test_gnm_runtime_abi tests/test_gnm_runtime_abi.cpp)
target_include_directories(test_gnm_runtime_abi PRIVATE platform/ps4/gcn8)
add_test(NAME gnm_runtime_abi_contract COMMAND test_gnm_runtime_abi)

add_executable(test_gnm_runtime_resolver tests/test_gnm_runtime_resolver.cpp)
target_include_directories(test_gnm_runtime_resolver PRIVATE platform/ps4/gcn8)
add_test(NAME gnm_runtime_resolver_contract COMMAND test_gnm_runtime_resolver)

add_library(pne_detour_transaction_core
    platform/ps4/goldhen/plugin/detour_transaction_core.cpp
)
target_include_directories(pne_detour_transaction_core PUBLIC
    platform/ps4/goldhen/plugin
)
add_executable(test_detour_transaction_core tests/test_detour_transaction_core.cpp)
target_link_libraries(test_detour_transaction_core PRIVATE pne_detour_transaction_core)
add_test(NAME detour_transaction_core COMMAND test_detour_transaction_core)
