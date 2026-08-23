from collections.abc import Callable
import logging
from pathlib import Path
import re
from string import ascii_letters, digits
import subprocess
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_ENABLE_FULL_PRINTF,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_VARIANT,
    CONF_VERSION,
    CONF_WATCHDOG_TIMEOUT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_RP2,
    ThreadModel,
)
from esphome.core import (
    CORE,
    CoroPriority,
    EsphomeCore,
    EsphomeError,
    coroutine_with_priority,
)
from esphome.core.config import BOARD_MAX_LENGTH
from esphome.helpers import copy_file_if_changed, read_file, write_file_if_changed
from esphome.platformio.toolchain import copy_ccache_script
from esphome.storage_json import StorageJSON
from esphome.types import ConfigType

from . import boards
from .const import (
    KEY_BOARD,
    KEY_LWIP_OPTS,
    KEY_PIO_FILES,
    KEY_RP2,
    KEY_VARIANT,
    MCU_TO_VARIANT,
    STANDARD_BOARDS,
    VARIANT_FRIENDLY,
    VARIANTS,
    rp2_ns,
)

# force import gpio to register pin schema
from .gpio import rp2_pin_to_code  # noqa: F401

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jesserockz"]
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True

# Legacy top-level YAML keys that route here. The framework
# (esphome/loader.py + esphome/config.py) handles both the deprecation
# warning and the key-rename pass; this declaration is the only place a
# component needs to opt in. See ComponentManifest.aliases for details.
ALIASES = ["rp2040"]
ALIAS_REMOVAL_VERSION = "2027.7.0"


def get_board() -> str:
    """Return the configured board name."""
    return CORE.data[KEY_RP2][KEY_BOARD]


def board_has_wifi() -> bool:
    """Return True if the configured board has WiFi (CYW43 wireless chip).

    Returns True for unknown/custom boards to avoid rejecting valid
    configurations for boards not in the generated list.
    """
    return board_id_has_wifi(get_board())


def board_id_has_wifi(board_id: str) -> bool:
    """Return True if *board_id* has WiFi (CYW43 wireless chip).

    Returns True for unknown/custom boards to avoid rejecting valid
    configurations for boards not in the generated list.

    Used by device-builder (esphome/device-builder) — separate
    explicit-arg helper so callers outside the compile pipeline
    don't need ``CORE`` set up to query the board map. Please keep
    the signature stable.
    """
    board_info = boards.BOARDS.get(board_id)
    if board_info is None:
        return True
    return board_info.get("wifi", False)


def set_core_data(config: ConfigType) -> ConfigType:
    CORE.data[KEY_RP2] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_RP2
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_RP2][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_RP2][KEY_VARIANT] = config[CONF_VARIANT]

    CORE.data[KEY_RP2][KEY_PIO_FILES] = {}

    return config


def get_rp2040_variant(core_obj: EsphomeCore | None = None) -> str:
    return (core_obj or CORE).data[KEY_RP2][KEY_VARIANT]


def only_on_variant(
    *,
    supported: str | list[str] | None = None,
    unsupported: str | list[str] | None = None,
    msg_prefix: str = "This feature",
) -> Callable[[Any], Any]:
    """Config validator for features only available on some RP2040 variants."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj: Any) -> Any:
        if not CORE.is_rp2:
            raise cv.Invalid(f"{msg_prefix} is only available on RP2040")
        variant = get_rp2040_variant()
        if supported is not None and variant not in supported:
            raise cv.Invalid(
                f"{msg_prefix} is only available on {', '.join(supported)}"
            )
        if unsupported is not None and variant in unsupported:
            raise cv.Invalid(
                f"{msg_prefix} is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


def get_download_types(storage_json: StorageJSON) -> list[dict[str, str]]:
    """Binary-download entries for a built RP2040 firmware.

    Used by device-builder (esphome/device-builder), via
    ``importlib.import_module(f"esphome.components.{platform}")``
    then ``module.get_download_types(storage)``. The contract is
    "returns ``list[dict]`` with at least ``title`` /
    ``description`` / ``file`` / ``download`` keys"; please keep
    the shape stable so the download panel
    doesn't have to special-case per-platform schemas.
    """
    # No recorded firmware path means nothing was built; no downloads.
    if storage_json.firmware_bin_path is None:
        return []
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
    # The framework-arduinopico package is no longer published to the PlatformIO
    # registry, so install the framework straight from the GitHub release
    return f"https://github.com/earlephilhower/arduino-pico/releases/download/{ver}/rp2040-{ver}.zip"


def _parse_platform_version(value: Any) -> str:
    value = cv.string(value)
    if value.startswith("http"):
        return value

    return f"https://github.com/maxgerhardt/platform-raspberrypi.git#{value}"


# NOTE: Keep this in mind when updating the recommended version:
#  * The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)

# The default/recommended arduino framework version
#  - https://github.com/earlephilhower/arduino-pico/releases
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(6, 0, 0)

# The raspberrypi platform version to use for arduino frameworks
#  - https://github.com/maxgerhardt/platform-raspberrypi/tags
# develop-branch commit carrying the arduino-pico 6.0.0 / pico-quick-toolchain
# 5.0.0 (GCC 16.1) update; replace with a release tag when one is cut
RECOMMENDED_ARDUINO_PLATFORM_VERSION = "9c167c6b8aac4f4cfa6d55a0c4e5b848795150c0"


def _arduino_check_versions(value: ConfigType) -> ConfigType:
    value = value.copy()
    lookups = {
        "dev": (cv.Version(6, 0, 0), "https://github.com/earlephilhower/arduino-pico"),
        "latest": (cv.Version(6, 0, 0), None),
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
            cv.Optional(
                CONF_SOURCE, visibility=cv.Visibility.YAML_ONLY
            ): cv.string_strict,
            cv.Optional(
                CONF_PLATFORM_VERSION, visibility=cv.Visibility.YAML_ONLY
            ): _parse_platform_version,
        }
    ),
    _arduino_check_versions,
)


def _detect_variant(value: ConfigType) -> ConfigType:
    value = value.copy()
    board: str | None = value.get(CONF_BOARD)
    variant: str | None = value.get(CONF_VARIANT)

    if board is None:
        # `cv.has_at_least_one_key` guarantees variant is set here.
        board = STANDARD_BOARDS[variant]
        value[CONF_BOARD] = board

    board_info = boards.BOARDS.get(board)
    if board_info is None:
        if variant is None:
            raise cv.Invalid(
                "This board is unknown; please specify the chip variant using "
                f"the '{CONF_VARIANT}' option.",
                path=[CONF_BOARD],
            )
        _LOGGER.warning(
            "This board is unknown; the specified variant '%s' will be used "
            "but this may not work as expected.",
            variant,
        )
    else:
        board_variant = MCU_TO_VARIANT[board_info["mcu"]]
        if variant is None:
            variant = board_variant
        elif variant != board_variant:
            raise cv.Invalid(
                f"Option '{CONF_VARIANT}' ({variant}) does not match the "
                f"selected board '{board}' ({board_variant}).",
                path=[CONF_VARIANT],
            )

    value[CONF_VARIANT] = variant
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_BOARD): cv.All(
                cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
            ),
            cv.Optional(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
            cv.Optional(CONF_FRAMEWORK, default={}): ARDUINO_FRAMEWORK_SCHEMA,
            cv.Optional(CONF_WATCHDOG_TIMEOUT, default="8388ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(milliseconds=8388)),
            ),
            cv.Optional(CONF_ENABLE_FULL_PRINTF, default=False): cv.boolean,
        }
    ),
    cv.has_at_least_one_key(CONF_BOARD, CONF_VARIANT),
    _detect_variant,
    set_core_data,
)


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    cg.add(rp2_ns.setup_preferences())

    # Allow LDF to properly discover dependency including those in preprocessor
    # conditionals
    cg.add_platformio_option("lib_ldf_mode", "chain+")
    cg.add_platformio_option("lib_compat_mode", "strict")
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_RP2")
    # USE_RP2040 kept defined as a backwards-compat alias for external
    # custom components that may still test for it. Internal code uses
    # USE_RP2 (the canonical name for the RP2 chip family — covers
    # RP2040, RP2350, and any future RP2-series chips).
    cg.add_build_flag("-DUSE_RP2040")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    cg.set_cpp_standard("gnu++20")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    variant = config[CONF_VARIANT]
    cg.add_build_flag(f"-DUSE_RP2040_VARIANT_{variant}")
    cg.add_define("ESPHOME_VARIANT", VARIANT_FRIENDLY[variant])
    cg.add_define(ThreadModel.SINGLE)

    cg.add_platformio_option("extra_scripts", ["pre:ccache.py", "post:post_build.py"])

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_RP2_FRAMEWORK_ARDUINO")
    cg.add_build_flag("-DUSE_RP2040_FRAMEWORK_ARDUINO")  # back-compat alias
    # cg.add_build_flag("-DPICO_BOARD=pico_w")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [
            f"earlephilhower/framework-arduinopico@{conf[CONF_SOURCE]}",
        ],
    )

    # newlib-nano is the default libc for the arduino-pico toolchain and its
    # printf silently drops %f unless _printf_float is force-linked. Components
    # use %f widely in logging, so pull it in.
    cg.add_build_flag("-Wl,-u,_printf_float")

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

    cg.add_define("USE_RP2_WATCHDOG_TIMEOUT", config[CONF_WATCHDOG_TIMEOUT])
    cg.add_define(
        "USE_RP2040_WATCHDOG_TIMEOUT", config[CONF_WATCHDOG_TIMEOUT]
    )  # back-compat alias
    cg.add_define("USE_RP2_CRASH_HANDLER")
    cg.add_define("USE_RP2040_CRASH_HANDLER")  # back-compat alias

    _configure_lwip()


# --- lwIP sizing. See _configure_lwip() for the platform comparison table. ---

# TCP_SND_BUF: 4×MSS=5,840 matches ESP32. Down from arduino-pico's 8×MSS.
# ESPAsyncWebServer allocates malloc(tcp_sndbuf()) per response chunk.
LWIP_TCP_SND_BUF = "(4*TCP_MSS)"

# TCP_WND: receive window. 4×MSS matches ESP32. Down from arduino-pico's 8×MSS.
LWIP_TCP_WND = "(4*TCP_MSS)"

# TCP_SND_QUEUELEN: max pbufs queued per PCB for the send buffer
# ESP-IDF formula: (4 * TCP_SND_BUF + (TCP_MSS - 1)) / TCP_MSS
# With 4×MSS: (4*5840 + 1459) / 1460 = 17 — match ESP32
LWIP_TCP_SND_QUEUELEN = 17

# MEMP_NUM_TCP_SEG: pool shared by every PCB, so it must not be the per-PCB
# queue length — lwIP's sanity check only demands >=, the floor for a single
# connection. 2× lets two PCBs fill up before the rest see ERR_MEM. Measured
# at 20 bytes per entry, so under 700 bytes total.
LWIP_MEMP_NUM_TCP_SEG = 2 * LWIP_TCP_SND_QUEUELEN

# PBUF_POOL_SIZE: RP2040 has 264KB RAM, more generous than LibreTiny.
# 16 matches ESP32 (vs arduino-pico's 24). Receive side only; the send path
# copies into PBUF_RAM out of MEM_SIZE.
LWIP_PBUF_POOL_SIZE = 16

# MEM_SIZE: lwIP heap backing PBUF_RAM, where tcp_write() copies outgoing
# data. TCP_OVERSIZE defaults to TCP_MSS, so each queued segment takes a full
# MSS block whatever was written (pbuf 16 + PBUF_TRANSPORT 54 + MSS 1460 +
# block header ≈ 1.5KB); a PCB at a full TCP_SND_BUF holds four, ~6KB.
#
# Two of those is ~12KB of arduino-pico's 16KB heap and already fails: mem.c
# is first-fit, so a *contiguous* 1.5KB block must be free, and at 75%
# occupancy interleaved with ARP/DHCP/DNS/mDNS the largest run collapses well
# before the total does — hence the intermittent failures. With rp2's
# max_connections of 4, a third sender has nothing left.
#
# 32KB is arduino-pico's own next tier (__LWIP_MEMMULT=2 boards).
# Must stay under 64000 or lwIP widens mem_size_t to u32_t.
LWIP_MEM_SIZE = 32768


def build_lwip_defines(
    tcp_sockets: int, udp_sockets: int, listening_tcp: int
) -> dict[str, str]:
    """Render the lwIP override values for the Jinja2 template.

    The template uses #include_next to chain to the framework's original
    lwipopts.h, then #undef/#define only these. Split out from
    _configure_lwip() so the values that actually reach the generated header
    can be checked without standing up CORE.

    Both malloc flags stay 0 (framework defaults); see _configure_lwip(). The
    static pools are the only IRQ-safe allocator on this platform, so the fix
    is to size them correctly rather than to make them dynamic.
    """
    return {
        "TCP_SND_BUF": LWIP_TCP_SND_BUF,
        "TCP_WND": LWIP_TCP_WND,
        "TCP_SND_QUEUELEN": str(LWIP_TCP_SND_QUEUELEN),
        "MEM_SIZE": str(LWIP_MEM_SIZE),
        "MEMP_NUM_TCP_SEG": str(LWIP_MEMP_NUM_TCP_SEG),
        "PBUF_POOL_SIZE": str(LWIP_PBUF_POOL_SIZE),
        "MEMP_NUM_TCP_PCB": str(tcp_sockets),
        "MEMP_NUM_TCP_PCB_LISTEN": str(listening_tcp),
        "MEMP_NUM_UDP_PCB": str(udp_sockets),
    }


def _configure_lwip() -> None:
    """Configure lwIP options for RP2040 by generating a custom lwipopts.h.

    Arduino-pico's lwipopts.h has no #ifndef guards, so -D flags cannot override
    its settings. Instead, we generate a replacement lwipopts.h and place it in an
    include directory that shadows the framework's version.

    lwIP is compiled from source on RP2040 (not pre-built), so our replacement
    header fully controls the compiled lwIP behavior.

    RP2040 uses NO_SYS=1 (polling, no RTOS thread), LWIP_SOCKET=0, LWIP_NETCONN=0.
    DHCP/DNS use raw udp_new() which allocates from MEMP_NUM_UDP_PCB.

    Comparison of arduino-pico defaults vs ESPHome targets (TCP_MSS=1460):

    Setting                   ESP8266  ESP32  arduino-pico  New
    ────────────────────────────────────────────────────────────────
    TCP_SND_BUF               2×MSS   4×MSS  8×MSS         4×MSS
    TCP_WND                   4×MSS   4×MSS  8×MSS         4×MSS
    TCP_SND_QUEUELEN          ~8      17     32            17
    MEM_LIBC_MALLOC           1       1      0             0*
    MEMP_MEM_MALLOC           1       1      0             0**
    MEM_SIZE                  N/A***  N/A*** 16KB          32KB
    PBUF_POOL_SIZE            10      16     24            16
    MEMP_NUM_TCP_SEG          10      16     32            34****
    MEMP_NUM_TCP_PCB          5       16     5             dynamic
    MEMP_NUM_TCP_PCB_LISTEN   4       16     8*****        dynamic
    MEMP_NUM_UDP_PCB          4       16     7             dynamic

    * MEM_LIBC_MALLOC must stay 0: arduino-pico uses
      PICO_CYW43_ARCH_THREADSAFE_BACKGROUND which runs lwIP callbacks from
      a low-priority pendsv IRQ. The pico-sdk explicitly blocks
      MEM_LIBC_MALLOC=1 because libc malloc uses mutexes (unsafe in IRQ).
    ** MEMP_MEM_MALLOC must stay 0 for IRQ safety, not size. memp_malloc()
      pops the pool free list inside SYS_ARCH_PROTECT, but lwIP's heap takes
      its protection from LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT (default 0),
      so under NO_SYS=1 mem_malloc()/mem_free() are unprotected — and memp.c
      calls mem_malloc() outside the guard anyway. RX pbufs would then be
      allocated from the pendsv IRQ on the same unguarded free list the main
      loop uses for tcp_write(). Tried on hardware: faults within seconds on
      CYW43. Ethernet survives only because it polls from the main loop.
    *** ESP8266/ESP32 ship MEMP_MEM_MALLOC=1, so their pool entries come from
      the heap on demand and MEMP_NUM_*/PBUF_POOL_SIZE are labels, not caps
      (MEM_LIBC_MALLOC=1 points that heap at the system heap). Both flags are
      0 here, so ours are hard limits; don't copy their numbers.
    **** MEMP_NUM_TCP_SEG is *global* while TCP_SND_QUEUELEN is *per-PCB*, so
      sizing it to the per-PCB value lets one busy connection drain it for
      every other. 2× covers two PCBs; MEM_SIZE is the real limit past that.
    ***** opt.h default; arduino-pico doesn't override MEMP_NUM_TCP_PCB_LISTEN.
    "dynamic" = auto-calculated from component socket registrations via
    socket.get_socket_counts() with minimums of 8 TCP / 6 UDP / 2 TCP_LISTEN.
    """
    from esphome.components.socket import (
        MIN_TCP_LISTEN_SOCKETS,
        MIN_TCP_SOCKETS,
        MIN_UDP_SOCKETS,
        get_socket_counts,
    )

    sc = get_socket_counts()
    # Apply platform minimums — ensure headroom for ESPHome's needs
    tcp_sockets = max(MIN_TCP_SOCKETS, sc.tcp)
    udp_sockets = max(MIN_UDP_SOCKETS, sc.udp)
    # RP2040 has more RAM (264KB) than most LibreTiny boards, so DHCP/DNS
    # UDP PCBs (2) are absorbed by the generous minimum of 6.
    listening_tcp = max(MIN_TCP_LISTEN_SOCKETS, sc.tcp_listen)

    lwip_defines = build_lwip_defines(tcp_sockets, udp_sockets, listening_tcp)

    # Store for copy_files() to generate the header
    CORE.data[KEY_RP2][KEY_LWIP_OPTS] = lwip_defines

    # Add a pre-build extra script that injects our lwip_override directory
    # into CCFLAGS so our lwipopts.h shadows the framework's version.
    # Regular build_flags (-I/-isystem) come after -iwithprefixbefore in GCC's
    # search order, so we must prepend via an extra_scripts hook.
    cg.add_platformio_option("extra_scripts", ["pre:inject_lwip_include.py"])

    tcp_min = " (min)" if tcp_sockets > sc.tcp else ""
    udp_min = " (min)" if udp_sockets > sc.udp else ""
    listen_min = " (min)" if listening_tcp > sc.tcp_listen else ""
    _LOGGER.info(
        "Configuring lwIP: %d byte heap; TCP=%d%s [%s], UDP=%d%s [%s], TCP_LISTEN=%d%s [%s]",
        LWIP_MEM_SIZE,
        tcp_sockets,
        tcp_min,
        sc.tcp_details,
        udp_sockets,
        udp_min,
        sc.udp_details,
        listening_tcp,
        listen_min,
        sc.tcp_listen_details,
    )


def _generate_lwipopts_h() -> None:
    """Generate a custom lwipopts.h that shadows the framework's version.

    Uses Jinja2 to render the template with the lwIP defines calculated
    during code generation. The generated header is placed in lwip_override/
    in the build directory, and a pre-build script injects this directory
    into the compiler include path before the framework's own include dir.
    """
    from jinja2 import Environment, StrictUndefined

    lwip_defines = CORE.data[KEY_RP2].get(KEY_LWIP_OPTS)
    if not lwip_defines:
        return

    # Read the template via pathlib and render from a string rather than using
    # FileSystemLoader. jinja2's loader joins the search path with posixpath, which
    # breaks on Windows extended-length paths (\\?\C:\...) where forward slashes are
    # not accepted, causing a spurious TemplateNotFound (see issue #16732).
    template_text = (Path(__file__).parent / "lwipopts.h.jinja").read_text(
        encoding="utf-8"
    )
    # StrictUndefined: a placeholder with no value would otherwise render
    # empty, emitting a bare #define that compiles and silently means
    # something else in lwIP's config.
    jinja_env = Environment(keep_trailing_newline=True, undefined=StrictUndefined)
    template = jinja_env.from_string(template_text)
    content = template.render(**lwip_defines)

    lwip_dir = CORE.relative_build_path("lwip_override")
    lwip_dir.mkdir(parents=True, exist_ok=True)
    write_file_if_changed(lwip_dir / "lwipopts.h", content)


def add_pio_file(component: str, key: str, data: str) -> None:
    try:
        cv.validate_id_name(key)
    except cv.Invalid as e:
        raise EsphomeError(
            f"[{component}] Invalid PIO key: {key}. Allowed characters: [{ascii_letters}{digits}_]\nPlease report an issue https://github.com/esphome/esphome/issues"
        ) from e
    CORE.data[KEY_RP2][KEY_PIO_FILES][key] = data


def generate_pio_files() -> bool:
    import shutil

    shutil.rmtree(CORE.relative_build_path("src/pio"), ignore_errors=True)

    includes: list[str] = []
    files = CORE.data[KEY_RP2][KEY_PIO_FILES]
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
def copy_files() -> None:
    dir = Path(__file__).parent
    post_build_file = dir / "post_build.py.script"
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
    inject_lwip_file = dir / "inject_lwip_include.py.script"
    copy_file_if_changed(
        inject_lwip_file,
        CORE.relative_build_path("inject_lwip_include.py"),
    )
    copy_ccache_script()
    _generate_lwipopts_h()
    if generate_pio_files():
        path = CORE.relative_src_path("esphome.h")
        content = read_file(path).rstrip("\n")
        write_file_if_changed(path, content + '\n#include "pio_includes.h"\n')


# RP2040 crash handler stacktrace decoding
# Matches output from esphome/components/rp2/crash_handler.cpp
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


def process_stacktrace(config: ConfigType, line: str, backtrace_state: bool) -> bool:
    """Decode RP2040 crash handler output using addr2line."""
    if _CRASH_RE.search(line):
        _LOGGER.error("RP2040 crash detected - decoding addresses")
        return True

    if backtrace_state:
        if match := _CRASH_ADDR_RE.search(line):
            from esphome.platformio.toolchain import get_idedata

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
