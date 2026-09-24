idf_component_register(SRCS "placeholder.c" INCLUDE_DIRS "include")
if(CMAKE_BUILD_EARLY_EXPANSION)
    return()
endif()

if(CONFIG_IDF_TARGET_ESP32C6)
    set(RUST_TARGET riscv32imac-unknown-none-elf)
elseif(CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32S3)
    if(CONFIG_IDF_TARGET_ESP32)
        set(RUST_TARGET xtensa-esp32-none-elf)
    else()
        set(RUST_TARGET xtensa-esp32s3-none-elf)
    endif()
else()
    message(FATAL_ERROR "The Statime adapter currently supports ESP32, ESP32-S3 and ESP32-C6. ESP32-C3 lacks the atomics required by the pinned dependency.")
endif()

file(READ "${PROJECT_DIR}/time_discipline.json" TIME_DISCIPLINE_CONFIG)
string(JSON TIME_DISCIPLINE_PYTHON GET "${TIME_DISCIPLINE_CONFIG}" python)

set(RUST_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/cargo/${RUST_TARGET}/release/libesphome_time_discipline.a")
add_custom_target(time_discipline_cargo
    COMMAND "${TIME_DISCIPLINE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/build.py"
        --target "${RUST_TARGET}" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/cargo"
    BYPRODUCTS "${RUST_ARCHIVE}"
    VERBATIM)
add_prebuilt_library(time_discipline_archive "${RUST_ARCHIVE}")
add_dependencies(time_discipline_archive time_discipline_cargo)
target_link_libraries(${COMPONENT_LIB} PRIVATE time_discipline_archive)
