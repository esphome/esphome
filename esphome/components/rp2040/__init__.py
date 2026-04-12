import logging
from pathlib import Path
import re
from string import ascii_letters, digits
import subprocess

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_ENABLE_FULL_PRINTF,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_VERSION,
    CONF_WATCHDOG_TIMEOUT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_RP2040,
    ThreadModel,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.core.config import BOARD_MAX_LENGTH
from esphome.helpers import copy_file_if_changed, read_file, write_file_if_changed

from . import boards
from .const import KEY_BOARD, KEY_PIO_FILES, KEY_RP2040, rp2040_ns

# force import gpio to register pin schema
from .gpio import rp2040_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jesserockz"]
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True


def get_board() -> str:
    """Return the configured board name."""
    return CORE.data[KEY_RP2040][KEY_BOARD]


def board_has_wifi() -> bool:
    """Return True if the configured board has WiFi (CYW43 wireless chip).

    Returns True for unknown/custom boards to avoid rejecting valid
    configurations for boards not in the generated list.
    """
    board_info = boards.BOARDS.get(get_board())
    if board_info is None:
        return True
    return board_info.get("wifi", False)


def set_core_data(config):
    CORE.data[KEY_RP2040] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_RP2040
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_RP2040][KEY_BOARD] = config[CONF_BOARD]

    CORE.data[KEY_RP2040][KEY_PIO_FILES] = {}

    return config


def get_download_types(storage_json):
    return [
        {
            "title": "UF2 factory format",
            "description": "For copying to RP2040 over USB.",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.factory.uf2",
        },
        {
            "title": "OTA format",
            "description": "For OTA updating a device.",
            "file": "firmware.ota.bin",
            "download": f"{storage_json.name}.ota.bin",
        },
    ]


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # The most recent releases have not been uploaded to platformio so grabbing them directly from
    # the GitHub release is one path forward for now.
    return f"https://github.com/earlephilhower/arduino-pico/releases/download/{ver}/rp2040-{ver}.zip"

    # format the given arduino (https://github.com/earlephilhower/arduino-pico/releases) version to
    # a PIO earlephilhower/framework-arduinopico value
    # List of package versions: https://api.registry.platformio.org/v3/packages/earlephilhower/tool/framework-arduinopico
    # return f"~1.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def _parse_platform_version(value):
    value = cv.string(value)
    if value.startswith("http"):
        return value

    return f"https://github.com/maxgerhardt/platform-raspberrypi.git#{value}"


# NOTE: Keep this in mind when updating the recommended version:
#  * The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/earlephilhower/arduino-pico/releases
#  - https://api.registry.platformio.org/v3/packages/earlephilhower/tool/framework-arduinopico
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(5, 5, 1)

# The raspberrypi platform version to use for arduino frameworks
#  - https://github.com/maxgerhardt/platform-raspberrypi/tags
RECOMMENDED_ARDUINO_PLATFORM_VERSION = "v1.4.0-gcc14-arduinopico460"


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(5, 5, 1), "https://github.com/earlephilhower/arduino-pico"),
        "latest": (cv.Version(5, 5, 1), None),
        "recommended": (RECOMMENDED_ARDUINO_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_arduino_version(version)

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION,
        _parse_platform_version(RECOMMENDED_ARDUINO_PLATFORM_VERSION),
    )

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one."
        )

    return value


ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _arduino_check_versions,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
            ),
            cv.Optional(CONF_FRAMEWORK, default={}): ARDUINO_FRAMEWORK_SCHEMA,
            cv.Optional(CONF_WATCHDOG_TIMEOUT, default="8388ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=8388)),
            ),
            cv.Optional(CONF_ENABLE_FULL_PRINTF, default=False): cv.boolean,
        }
    ),
    set_core_data,
)


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config):
    cg.add(rp2040_ns.setup_preferences())

    # Allow LDF to properly discover dependency including those in preprocessor
    # conditionals
    cg.add_platformio_option("lib_ldf_mode", "chain+")
    cg.add_platformio_option("lib_compat_mode", "strict")
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_RP2040")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2040")
    cg.add_define(ThreadModel.SINGLE)

    cg.add_platformio_option("extra_scripts", ["post:post_build.py"])

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_RP2040_FRAMEWORK_ARDUINO")
    # cg.add_build_flag("-DPICO_BOARD=pico_w")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [
            f"earlephilhower/framework-arduinopico@{conf[CONF_SOURCE]}",
        ],
    )

    # Wrap FILE*-based printf functions to eliminate newlib's _vfprintf_r
    # (~9.2 KB). See printf_stubs.cpp for implementation.
    if config.get(CONF_ENABLE_FULL_PRINTF):
        cg.add_define("USE_FULL_PRINTF")
    else:
        for symbol in ("vprintf", "printf", "fprintf"):
            cg.add_build_flag(f"-Wl,--wrap={symbol}")

    cg.add_platformio_option("board_build.core", "earlephilhower")
    # In testing mode, use all flash for sketch to allow linking grouped component tests.
    # Real RP2040 hardware uses 1MB filesystem + 1MB sketch, but CI tests may combine
    # many components that exceed the 1MB sketch partition.
    cg.add_platformio_option(
        "board_build.filesystem_size", "0m" if CORE.testing_mode else "1m"
    )

    ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    cg.add_define(
        "USE_ARDUINO_VERSION_CODE",
        cg.RawExpression(f"VERSION_CODE({ver.major}, {ver.minor}, {ver.patch})"),
    )

    cg.add_define("USE_RP2040_WATCHDOG_TIMEOUT", config[CONF_WATCHDOG_TIMEOUT])
    cg.add_define("USE_RP2040_CRASH_HANDLER")


def add_pio_file(component: str, key: str, data: str):
    try:
        cv.validate_id_name(key)
    except cv.Invalid as e:
        raise EsphomeError(
            f"[{component}] Invalid PIO key: {key}. Allowed characters: [{ascii_letters}{digits}_]\nPlease report an issue https://github.com/esphome/esphome/issues"
        ) from e
    CORE.data[KEY_RP2040][KEY_PIO_FILES][key] = data


def generate_pio_files() -> bool:
    import shutil

    shutil.rmtree(CORE.relative_build_path("src/pio"), ignore_errors=True)

    includes: list[str] = []
    files = CORE.data[KEY_RP2040][KEY_PIO_FILES]
    if not files:
        return False
    for key, data in files.items():
        pio_path = CORE.build_path / "src" / "pio" / f"{key}.pio"
        pio_path.parent.mkdir(parents=True, exist_ok=True)
        write_file_if_changed(pio_path, data)
        includes.append(f"pio/{key}.pio.h")

    write_file_if_changed(
        CORE.relative_build_path("src/pio_includes.h"),
        "#pragma once\n" + "\n".join([f'#include "{include}"' for include in includes]),
    )

    dir = Path(__file__).parent
    build_pio_file = dir / "build_pio.py.script"
    copy_file_if_changed(
        build_pio_file,
        CORE.relative_build_path("build_pio.py"),
    )

    return True


# Called by writer.py
def copy_files():
    dir = Path(__file__).parent
    post_build_file = dir / "post_build.py.script"
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
    if generate_pio_files():
        path = CORE.relative_src_path("esphome.h")
        content = read_file(path).rstrip("\n")
        write_file_if_changed(path, content + '\n#include "pio_includes.h"\n')


# RP2040 crash handler stacktrace decoding
# Matches output from esphome/components/rp2040/crash_handler.cpp
_CRASH_RE = re.compile(r"CRASH DETECTED ON PREVIOUS BOOT")
_CRASH_ADDR_RE = re.compile(
    r"(?:PC|LR|BT\d):\s+(0x[0-9a-fA-F]{8})\s+\((?:fault location|return address|stack backtrace)\)"
)


def _addr2line(tool: str, elf: Path, addr: str) -> str:
    try:
        result = subprocess.run(
            [tool, "-pfiaC", "-e", str(elf), addr],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return f"{addr} (decode failed)"


def process_stacktrace(config, line: str, backtrace_state: bool) -> bool:
    """Decode RP2040 crash handler output using addr2line."""
    if _CRASH_RE.search(line):
        _LOGGER.error("RP2040 crash detected - decoding addresses")
        return True

    if backtrace_state:
        if match := _CRASH_ADDR_RE.search(line):
            from esphome.platformio_api import get_idedata

            idedata = get_idedata(config)
            if idedata.addr2line_path:
                elf = idedata.firmware_elf_path
                if elf.exists():
                    decoded = _addr2line(idedata.addr2line_path, elf, match.group(1))
                    _LOGGER.error("  %s => %s", match.group(1), decoded)

        # Stop backtrace state after addr2line hint (last line of crash dump)
        if "addr2line" in line:
            return False

    return backtrace_state
