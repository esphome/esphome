idf_component_register(SRCS "placeholder.c" INCLUDE_DIRS "include")
if(CMAKE_BUILD_EARLY_EXPANSION)
    return()
endif()

set(RUST_TOOLCHAIN 1.98.1)
set(RUST_BUILD_STD_ARGS)
if(CONFIG_IDF_TARGET_ESP32C6)
    set(RUST_TARGET riscv32imac-unknown-none-elf)
elseif(CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32S3)
    set(RUST_TOOLCHAIN esp-1.97.0.0)
    set(RUST_BUILD_STD_ARGS -Zbuild-std=core)
    if(CONFIG_IDF_TARGET_ESP32)
        set(RUST_TARGET xtensa-esp32-none-elf)
    else()
        set(RUST_TARGET xtensa-esp32s3-none-elf)
    endif()
else()
    message(FATAL_ERROR "The Statime adapter currently supports ESP32, ESP32-S3 and ESP32-C6. ESP32-C3 lacks the atomics required by the pinned dependency.")
endif()

find_program(RUSTUP rustup REQUIRED)
execute_process(COMMAND "${RUSTUP}" which --toolchain "${RUST_TOOLCHAIN}" cargo
    OUTPUT_VARIABLE RUST_CARGO OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${RUSTUP}" which --toolchain "${RUST_TOOLCHAIN}" rustc
    OUTPUT_VARIABLE RUST_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)

set(RUST_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/cargo/${RUST_TARGET}/release/libesphome_time_discipline.a")
add_custom_target(time_discipline_cargo
    COMMAND "${CMAKE_COMMAND}" -E env "RUSTC=${RUST_COMPILER}"
        "${RUST_CARGO}" build --manifest-path "${CMAKE_CURRENT_LIST_DIR}/Cargo.toml"
        --target "${RUST_TARGET}" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/cargo"
        --release --locked ${RUST_BUILD_STD_ARGS}
    BYPRODUCTS "${RUST_ARCHIVE}"
    VERBATIM)
add_prebuilt_library(time_discipline_archive "${RUST_ARCHIVE}")
add_dependencies(time_discipline_archive time_discipline_cargo)
target_link_libraries(${COMPONENT_LIB} PRIVATE time_discipline_archive)
