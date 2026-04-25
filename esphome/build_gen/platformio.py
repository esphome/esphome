from esphome.const import __version__
from esphome.core import CORE
from esphome.helpers import mkdir_p, read_file, write_file_if_changed
from esphome.writer import find_begin_end, update_storage_json

INI_AUTO_GENERATE_BEGIN = "; ========== AUTO GENERATED CODE BEGIN ==========="
INI_AUTO_GENERATE_END = "; =========== AUTO GENERATED CODE END ============"

INI_BASE_FORMAT = (
    """; Auto generated code by esphome

[common]
lib_deps =
build_flags =
upload_flags =

""",
    """

""",
)


def format_ini(data: dict[str, str | list[str]]) -> str:
    content = ""
    for key, value in sorted(data.items()):
        if isinstance(value, list):
            content += f"{key} =\n"
            for x in value:
                content += f"    {x}\n"
        else:
            content += f"{key} = {value}\n"
    return content


def get_ini_content():
    CORE.add_platformio_option(
        "lib_deps",
        [x.as_lib_dep for x in CORE.platformio_libraries.values()]
        + ["${common.lib_deps}"],
    )
    # Sort to avoid changing build flags order
    CORE.add_platformio_option("build_flags", sorted(CORE.build_flags))

    # Sort to avoid changing build unflags order
    CORE.add_platformio_option("build_unflags", sorted(CORE.build_unflags))

    # Add extra scripts for C++ flags and cache metadata
    CORE.add_platformio_option(
        "extra_scripts",
        [f"pre:{CXX_FLAGS_FILE_NAME}", f"post:{NO_CACHE_FILE_NAME}"],
    )

    content = "[platformio]\n"
    content += f"description = ESPHome {__version__}\n"

    content += f"[env:{CORE.pioenv_name}]\n"
    content += format_ini(CORE.platformio_options)

    return content


def write_ini(content):
    update_storage_json()
    path = CORE.relative_build_path("platformio.ini")

    if path.is_file():
        text = read_file(path)
        content_format = find_begin_end(
            text, INI_AUTO_GENERATE_BEGIN, INI_AUTO_GENERATE_END
        )
    else:
        content_format = INI_BASE_FORMAT
    full_file = f"{content_format[0] + INI_AUTO_GENERATE_BEGIN}\n{content}"
    full_file += INI_AUTO_GENERATE_END + content_format[1]
    write_file_if_changed(path, full_file)


def write_project():
    mkdir_p(CORE.build_path)

    content = get_ini_content()
    write_ini(content)

    # Write extra scripts for C++ specific flags and cache metadata.
    write_cxx_flags_script()
    write_no_cache_script()


CXX_FLAGS_FILE_NAME = "cxx_flags.py"
CXX_FLAGS_FILE_CONTENTS = """# Auto-generated ESPHome script for C++ specific compiler flags
Import("env")

# Add C++ specific flags
"""

NO_CACHE_FILE_NAME = "no_cache.py"
NO_CACHE_FILE_CONTENTS = """# Auto-generated ESPHome script for SCons cache metadata
Import("env")

# Keep the shared SCons cache focused on reusable compiler outputs. Final
# firmware images and generated project entry points embed device names,
# build metadata, or absolute build paths, so caching them only stores
# per-device duplicates in the shared PlatformIO build cache.
for path in (
    "$BUILD_DIR/${PROGNAME}.elf",
    "$BUILD_DIR/${PROGNAME}.bin",
    "$BUILD_DIR/${PROGNAME}.factory.bin",
    "$BUILD_DIR/${PROGNAME}.ota.bin",
    "$BUILD_DIR/${PROGNAME}.uf2",
    "$BUILD_DIR/${PROGNAME}.hex",
    "$BUILD_DIR/raw_firmware.elf",
    "$BUILD_DIR/raw_firmware.bin",
    "$BUILD_DIR/firmware.uf2",
    "$BUILD_DIR/sections.ld",
    "$BUILD_DIR/esp-idf/esp_system/ld/sections.ld",
    "$BUILD_DIR/src/esphome/core/build_info_data.cpp.o",
    "$BUILD_DIR/src/main.cpp.o",
):
    env.NoCache(env.File(env.subst(path)))
"""


def write_cxx_flags_script() -> None:
    path = CORE.relative_build_path(CXX_FLAGS_FILE_NAME)
    contents = CXX_FLAGS_FILE_CONTENTS
    if not CORE.is_host:
        contents += 'env.Append(CXXFLAGS=["-Wno-volatile"])'
        contents += "\n"
    write_file_if_changed(path, contents)


def write_no_cache_script() -> None:
    path = CORE.relative_build_path(NO_CACHE_FILE_NAME)
    contents = NO_CACHE_FILE_CONTENTS
    write_file_if_changed(path, contents)
