cmake_minimum_required(VERSION 3.16)
project(high_performance_telemetry VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TELEMETRY_RING_BUFFER_CAPACITY "4096" CACHE STRING
    "Physical SPSC ring-buffer slots; must be a power of two and at least 2")
option(TELEMETRY_USE_MODULO_INDEXING
       "Use modulo-based ring indexing for performance comparison"
       OFF)

find_package(Threads REQUIRED)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(telemetry_core
    src/ring_buffer.cpp
    src/telemetry_stats.cpp
    src/packet_generator.cpp
    src/telemetry_logger.cpp
)
target_include_directories(telemetry_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_options(telemetry_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(telemetry_core PUBLIC Threads::Threads)
target_compile_definitions(telemetry_core PUBLIC
    TELEMETRY_RING_BUFFER_CAPACITY=${TELEMETRY_RING_BUFFER_CAPACITY})

if(TELEMETRY_USE_MODULO_INDEXING)
    target_compile_definitions(telemetry_core PRIVATE TELEMETRY_USE_MODULO_INDEXING=1)
endif()

add_executable(telemetry src/main.cpp)
target_link_libraries(telemetry PRIVATE telemetry_core)
target_compile_options(telemetry PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(ring_buffer_tests tests/ring_buffer_tests.cpp)
target_link_libraries(ring_buffer_tests PRIVATE telemetry_core)
target_compile_options(ring_buffer_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ring_buffer_tests COMMAND ring_buffer_tests)

add_executable(telemetry_stress_tests tests/stress_tests.cpp)
target_link_libraries(telemetry_stress_tests PRIVATE telemetry_core)
target_compile_options(telemetry_stress_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME telemetry_stress_tests COMMAND telemetry_stress_tests)

install(TARGETS telemetry
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS telemetry_core
    EXPORT HighPerformanceTelemetryTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HighPerformanceTelemetryConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/HighPerformanceTelemetryConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HighPerformanceTelemetryConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HighPerformanceTelemetry
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HighPerformanceTelemetryConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HighPerformanceTelemetryConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HighPerformanceTelemetry
)

install(EXPORT HighPerformanceTelemetryTargets
    FILE HighPerformanceTelemetryTargets.cmake
    NAMESPACE HighPerformanceTelemetry::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HighPerformanceTelemetry
)
