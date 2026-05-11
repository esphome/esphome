import logging
from pathlib import Path
import re
import subprocess

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_BOARD_FLASH_MODE,
    CONF_ENABLE_FULL_PRINTF,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP8266,
    ThreadModel,
)
from esphome.core import CORE, CoroPriority, Lambda, coroutine_with_priority
from esphome.core.config import BOARD_MAX_LENGTH
from esphome.helpers import copy_file_if_changed
from esphome.types import ConfigType

from .boards import BOARDS, ESP8266_LD_SCRIPTS
from .const import (
    CONF_EARLY_PIN_INIT,
    CONF_ENABLE_SERIAL,
    CONF_ENABLE_SERIAL1,
    CONF_RESTORE_FROM_FLASH,
    KEY_BOARD,
    KEY_ESP8266,
    KEY_FLASH_SIZE,
    KEY_PIN_INITIAL_STATES,
    KEY_SERIAL1_REQUIRED,
    KEY_SERIAL_REQUIRED,
    KEY_WAVEFORM_REQUIRED,
    enable_serial,
    enable_serial1,
    esp8266_ns,
)
from .gpio import PinInitialState, add_pin_initial_states_array

CONF_ENABLE_SCANF_FLOAT = "enable_scanf_float"
# Heuristically matches scanf/sscanf calls with float format specifiers.
# Standard scanf float conversions: %f %F %e %E %g %G %a %A
# With optional modifiers: %*f (suppression), %8f (width), %lf %Lf (length)
# Also matches non-standard patterns like %.2f as a heuristic — these are
# invalid in scanf but users may write them by analogy with printf.
# Uses [^;]*? to stay within a single statement, preventing false positives
# from e.g. sscanf(buf, "%d", &x); printf("%f", val);
_SCANF_FLOAT_RE = re.compile(r"scanf\s*\([^;]*?%[*\d.]*[hlL]*[feEgGaAF]")

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True


def lambdas_use_scanf_float(config: ConfigType) -> bool:
    """Check if any lambda in the config uses scanf with a float format specifier.

    Comments are stripped before matching to avoid false positives from
    commented-out code. The cost of a false positive is only ~8KB flash.
    """
    stack: list = [config]
    while stack:
        obj = stack.pop()
        if isinstance(obj, Lambda):
            src = obj.comment_remover(obj.value)
            if _SCANF_FLOAT_RE.search(src):
                return True
        elif isinstance(obj, dict):
            stack.extend(obj.values())
        elif isinstance(obj, list):
            stack.extend(obj)
    return False


def set_core_data(config):
    CORE.data[KEY_ESP8266] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_ESP8266
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_ESP8266][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ESP8266][KEY_PIN_INITIAL_STATES] = [
        PinInitialState() for _ in range(16)
    ]
    return config


def get_download_types(storage_json):
    """Binary-download entries for a built ESP8266 firmware.

    Used by:
    - esphome.dashboard (legacy "Download .bin" button)
    - device-builder (esphome/device-builder) — same dispatch via
      ``importlib.import_module(f"esphome.components.{platform}")``
      then ``module.get_download_types(storage)``. The contract is
      "returns ``list[dict]`` with at least ``title`` /
      ``description`` / ``file`` / ``download`` keys"; please keep
      the shape stable so the new dashboard's download panel
      doesn't have to special-case per-platform schemas.
    """
    return [
        {
            "title": "Standard format",
            "description": "For flashing ESP8266.",
            "file": "firmware.bin",
            "download": f"{storage_json.name}.bin",
        },
    ]


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/esp8266/Arduino/releases) version to
    # a PIO platformio/framework-arduinoespressif8266 value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif8266
    if ver <= cv.Version(2, 4, 1):
        return f"~1.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    if ver <= cv.Version(2, 6, 2):
        return f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/esp8266/Arduino/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif8266
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(3, 1, 2)
# The platformio/espressif8266 version to use for arduino 2 framework versions
#  - https://github.com/platformio/platform-espressif8266/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif8266
ARDUINO_2_PLATFORM_VERSION = cv.Version(2, 6, 3)
# for arduino 3 framework versions
ARDUINO_3_PLATFORM_VERSION = cv.Version(3, 2, 0)
# for arduino 4 framework versions
ARDUINO_4_PLATFORM_VERSION = cv.Version(4, 2, 1)


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(3, 1, 2), "https://github.com/esp8266/Arduino.git"),
        "latest": (cv.Version(3, 1, 2), None),
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

    platform_version = value.get(CONF_PLATFORM_VERSION)
    if platform_version is None:
        if version >= cv.Version(3, 1, 0):
            platform_version = _parse_platform_version(str(ARDUINO_4_PLATFORM_VERSION))
        elif version >= cv.Version(3, 0, 0):
            platform_version = _parse_platform_version(str(ARDUINO_3_PLATFORM_VERSION))
        elif version >= cv.Version(2, 5, 0):
            platform_version = _parse_platform_version(str(ARDUINO_2_PLATFORM_VERSION))
        else:
            platform_version = _parse_platform_version(str(cv.Version(1, 8, 0)))
    value[CONF_PLATFORM_VERSION] = platform_version

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/espressif8266@{value}"
    except cv.Invalid:
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


BUILD_FLASH_MODES = ["qio", "qout", "dio", "dout"]
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
            ),
            cv.Optional(CONF_FRAMEWORK, default={}): ARDUINO_FRAMEWORK_SCHEMA,
            cv.Optional(CONF_RESTORE_FROM_FLASH, default=False): cv.boolean,
            cv.Optional(CONF_EARLY_PIN_INIT, default=True): cv.boolean,
            cv.Optional(CONF_BOARD_FLASH_MODE, default="dout"): cv.one_of(
                *BUILD_FLASH_MODES, lower=True
            ),
            cv.Optional(CONF_ENABLE_SERIAL): cv.boolean,
            cv.Optional(CONF_ENABLE_SERIAL1): cv.boolean,
            cv.Optional(CONF_ENABLE_FULL_PRINTF, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_SCANF_FLOAT): cv.boolean,
        }
    ),
    set_core_data,
)


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config):
    cg.add(esp8266_ns.setup_preferences())

    cg.add_platformio_option("lib_ldf_mode", "off")
    cg.add_platformio_option("lib_compat_mode", "strict")

    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_ESP8266")
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP8266")
    cg.add_define(ThreadModel.SINGLE)
    cg.add_define("USE_ESP8266_CRASH_HANDLER")

    enable_scanf_float = config.get(CONF_ENABLE_SCANF_FLOAT)
    if enable_scanf_float is None and lambdas_use_scanf_float(CORE.config):
        enable_scanf_float = True
        _LOGGER.warning(
            "Lambda uses scanf with a float format specifier; "
            "enabling scanf float support (~8KB flash)"
        )

    extra_scripts = [
        "pre:testing_mode.py",
        "pre:exclude_updater.py",
        "pre:exclude_waveform.py",
    ]
    if not enable_scanf_float:
        extra_scripts.append("pre:remove_float_scanf.py")
    extra_scripts.append("post:post_build.py")
    cg.add_platformio_option("extra_scripts", extra_scripts)

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_ESP8266_FRAMEWORK_ARDUINO")
    cg.add_build_flag("-Wno-nonnull-compare")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [f"platformio/framework-arduinoespressif8266@{conf[CONF_SOURCE]}"],
    )

    # Default for platformio is LWIP2_LOW_MEMORY with:
    #  - MSS=536
    #  - LWIP_FEATURES enabled
    #     - this only adds some optional features like IP incoming packet reassembly and NAPT
    #       see also:
    #  https://github.com/esp8266/Arduino/blob/master/tools/sdk/lwip2/include/lwipopts.h

    # Instead we use LWIP2_HIGHER_BANDWIDTH_LOW_FLASH with:
    #  - MSS=1460
    #  - LWIP_FEATURES disabled (because we don't need them)
    # Other projects like Tasmota & ESPEasy also use this
    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")

    if config[CONF_RESTORE_FROM_FLASH]:
        cg.add_define("USE_ESP8266_PREFERENCES_FLASH")

    if config[CONF_EARLY_PIN_INIT]:
        cg.add_define("USE_ESP8266_EARLY_PIN_INIT")

    # Allow users to force-enable Serial objects for use in lambdas or external libraries
    if config.get(CONF_ENABLE_SERIAL):
        enable_serial()
    if config.get(CONF_ENABLE_SERIAL1):
        enable_serial1()

    # Arduino 2 has a non-standards conformant new that returns a nullptr instead of failing when
    # out of memory and exceptions are disabled. Since Arduino 2.6.0, this flag can be used to make
    # new abort instead. Use it so that OOM fails early (on allocation) instead of on dereference of
    # a NULL pointer (so the stacktrace makes more sense), and for consistency with Arduino 3,
    # which always aborts if exceptions are disabled.
    # For cases where nullptrs can be handled, use nothrow: `new (std::nothrow) T;`
    cg.add_build_flag("-DNEW_OOM_ABORT")

    # In testing mode, fake larger memory to allow linking grouped component tests
    # Real ESP8266 hardware only has 32KB IRAM and ~80KB RAM, but for CI testing
    # we pretend it has much larger memory to test that components compile together
    if CORE.testing_mode:
        cg.add_build_flag("-DESPHOME_TESTING_MODE")

    # Wrap FILE*-based printf functions to eliminate newlib's _vfiprintf_r
    # (~1.6 KB). See printf_stubs.cpp for implementation.
    if config.get(CONF_ENABLE_FULL_PRINTF):
        cg.add_define("USE_FULL_PRINTF")
    else:
        for symbol in ("vprintf", "printf", "fprintf"):
            cg.add_build_flag(f"-Wl,--wrap={symbol}")

    # Wrap Arduino's millis() so all callers (including Arduino libraries and ISR
    # handlers) use our fast accumulator instead of the expensive 4x 64-bit multiply
    # implementation in the Arduino ESP8266 core.
    cg.add_build_flag("-Wl,--wrap=millis")

    cg.add_platformio_option("board_build.flash_mode", config[CONF_BOARD_FLASH_MODE])

    ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    cg.add_define(
        "USE_ARDUINO_VERSION_CODE",
        cg.RawExpression(f"VERSION_CODE({ver.major}, {ver.minor}, {ver.patch})"),
    )

    if config[CONF_BOARD] in BOARDS:
        flash_size = BOARDS[config[CONF_BOARD]][KEY_FLASH_SIZE]
        ld_scripts = ESP8266_LD_SCRIPTS[flash_size]

        if ver <= cv.Version(2, 3, 0):
            # No ld script support
            ld_script = None
        elif ver <= cv.Version(2, 4, 2):
            # Old ld script path
            ld_script = ld_scripts[0]
        else:
            ld_script = ld_scripts[1]

        if ld_script is not None:
            cg.add_platformio_option("board_build.ldscript", ld_script)

    CORE.add_job(add_pin_initial_states_array)
    CORE.add_job(finalize_waveform_config)
    CORE.add_job(finalize_serial_config)


@coroutine_with_priority(CoroPriority.WORKAROUNDS)
async def finalize_waveform_config() -> None:
    """Add waveform stubs define if waveform is not required.

    This runs at WORKAROUNDS priority (-999) to ensure all components
    have had a chance to call require_waveform() first.
    """
    if not CORE.data.get(KEY_ESP8266, {}).get(KEY_WAVEFORM_REQUIRED, False):
        # No component needs waveform - enable stubs and exclude Arduino waveform code
        # Use build flag (visible to both C++ code and PlatformIO script)
        cg.add_build_flag("-DUSE_ESP8266_WAVEFORM_STUBS")


@coroutine_with_priority(CoroPriority.WORKAROUNDS)
async def finalize_serial_config() -> None:
    """Exclude unused Arduino Serial objects from the build.

    This runs at WORKAROUNDS priority (-999) to ensure all components
    have had a chance to call enable_serial() or enable_serial1() first.

    The Arduino ESP8266 core defines two global Serial objects (32 bytes each).
    By adding NO_GLOBAL_SERIAL or NO_GLOBAL_SERIAL1 build flags, we prevent
    unused Serial objects from being linked, saving 32 bytes each.
    """
    esp8266_data = CORE.data.get(KEY_ESP8266, {})
    if not esp8266_data.get(KEY_SERIAL_REQUIRED, False):
        cg.add_build_flag("-DNO_GLOBAL_SERIAL")
    if not esp8266_data.get(KEY_SERIAL1_REQUIRED, False):
        cg.add_build_flag("-DNO_GLOBAL_SERIAL1")


# Called by writer.py
def copy_files() -> None:
    dir = Path(__file__).parent
    post_build_file = dir / "post_build.py.script"
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
    testing_mode_file = dir / "testing_mode.py.script"
    copy_file_if_changed(
        testing_mode_file,
        CORE.relative_build_path("testing_mode.py"),
    )
    exclude_updater_file = dir / "exclude_updater.py.script"
    copy_file_if_changed(
        exclude_updater_file,
        CORE.relative_build_path("exclude_updater.py"),
    )
    exclude_waveform_file = dir / "exclude_waveform.py.script"
    copy_file_if_changed(
        exclude_waveform_file,
        CORE.relative_build_path("exclude_waveform.py"),
    )
    remove_float_scanf_file = dir / "remove_float_scanf.py.script"
    copy_file_if_changed(
        remove_float_scanf_file,
        CORE.relative_build_path("remove_float_scanf.py"),
    )


# ESP logs stack trace decoder, based on https://github.com/me-no-dev/EspExceptionDecoder
ESP8266_EXCEPTION_CODES = {
    0: "Illegal instruction (Is the flash damaged?)",
    1: "SYSCALL instruction",
    2: "InstructionFetchError: Processor internal physical address or data error during "
    "instruction fetch",
    3: "LoadStoreError: Processor internal physical address or data error during load or store",
    4: "Level1Interrupt: Level-1 interrupt as indicated by set level-1 bits in the INTERRUPT "
    "register",
    5: "Alloca: MOVSP instruction, if caller's registers are not in the register file",
    6: "Integer Divide By Zero",
    7: "reserved",
    8: "Privileged: Attempt to execute a privileged operation when CRING ? 0",
    9: "LoadStoreAlignmentCause: Load or store to an unaligned address",
    10: "reserved",
    11: "reserved",
    12: "InstrPIFDataError: PIF data error during instruction fetch",
    13: "LoadStorePIFDataError: Synchronous PIF data error during LoadStore access",
    14: "InstrPIFAddrError: PIF address error during instruction fetch",
    15: "LoadStorePIFAddrError: Synchronous PIF address error during LoadStore access",
    16: "InstTLBMiss: Error during Instruction TLB refill",
    17: "InstTLBMultiHit: Multiple instruction TLB entries matched",
    18: "InstFetchPrivilege: An instruction fetch referenced a virtual address at a ring level "
    "less than CRING",
    19: "reserved",
    20: "InstFetchProhibited: An instruction fetch referenced a page mapped with an attribute "
    "that does not permit instruction fetch",
    21: "reserved",
    22: "reserved",
    23: "reserved",
    24: "LoadStoreTLBMiss: Error during TLB refill for a load or store",
    25: "LoadStoreTLBMultiHit: Multiple TLB entries matched for a load or store",
    26: "LoadStorePrivilege: A load or store referenced a virtual address at a ring level less "
    "than ",
    27: "reserved",
    28: "Access to invalid address: LOAD (wild pointer?)",
    29: "Access to invalid address: STORE (wild pointer?)",
}


def _decode_pc(config, addr):
    from esphome.platformio import toolchain

    idedata = toolchain.get_idedata(config)
    if not idedata.addr2line_path or not idedata.firmware_elf_path:
        _LOGGER.debug("decode_pc no addr2line")
        return
    command = [idedata.addr2line_path, "-pfiaC", "-e", idedata.firmware_elf_path, addr]
    try:
        translation = subprocess.check_output(command, close_fds=False).decode().strip()
    except Exception:  # pylint: disable=broad-except
        _LOGGER.debug("Caught exception for command %s", command, exc_info=1)
        return

    if "?? ??:0" in translation:
        # Nothing useful
        return
    translation = translation.replace(" at ??:?", "").replace(":?", "")
    _LOGGER.warning("Decoded %s", translation)


def _parse_register(config, regex, line):
    match = regex.match(line)
    if match is not None:
        _decode_pc(config, match.group(1))


STACKTRACE_ESP8266_EXCEPTION_TYPE_RE = re.compile(r"[eE]xception \((\d+)\):")
STACKTRACE_ESP8266_PC_RE = re.compile(r"epc1=0x(4[0-9a-fA-F]{7})")
STACKTRACE_ESP8266_EXCVADDR_RE = re.compile(r"excvaddr=0x(4[0-9a-fA-F]{7})")
STACKTRACE_BAD_ALLOC_RE = re.compile(
    r"^last failed alloc call: (4[0-9a-fA-F]{7})\((\d+)\)$"
)
STACKTRACE_ESP8266_BACKTRACE_PC_RE = re.compile(r"4[0-9a-f]{7}")


def process_stacktrace(config, line, backtrace_state):
    line = line.strip()
    # ESP8266 Exception type
    match = re.match(STACKTRACE_ESP8266_EXCEPTION_TYPE_RE, line)
    if match is not None:
        code = int(match.group(1))
        _LOGGER.warning(
            "Exception type: %s", ESP8266_EXCEPTION_CODES.get(code, "unknown")
        )

    # ESP8266 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP8266_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP8266_EXCVADDR_RE, line)

    # bad alloc
    match = re.match(STACKTRACE_BAD_ALLOC_RE, line)
    if match is not None:
        _LOGGER.warning(
            "Memory allocation of %s bytes failed at %s", match.group(2), match.group(1)
        )
        _decode_pc(config, match.group(1))

    # ESP8266 multi-line backtrace
    if ">>>stack>>>" in line:
        # Start of backtrace
        backtrace_state = True
        _LOGGER.warning("Found stack trace! Trying to decode it")
    elif "<<<stack<<<" in line:
        # End of backtrace
        backtrace_state = False

    if backtrace_state:
        for addr in re.finditer(STACKTRACE_ESP8266_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    return backtrace_state
