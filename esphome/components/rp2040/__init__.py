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
from .const import KEY_BOARD, KEY_LWIP_OPTS, KEY_PIO_FILES, KEY_RP2040, rp2040_ns

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
    """Binary-download entries for a built RP2040 firmware.

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
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(5, 6, 0)

# The raspberrypi platform version to use for arduino frameworks
#  - https://github.com/maxgerhardt/platform-raspberrypi/tags
RECOMMENDED_ARDUINO_PLATFORM_VERSION = "v1.4.0-gcc14-arduinopico460"


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(5, 6, 0), "https://github.com/earlephilhower/arduino-pico"),
        "latest": (cv.Version(5, 6, 0), None),
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

    _configure_lwip()


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
    MEM_LIBC_MALLOC           1       1      0             0*
    MEMP_MEM_MALLOC           1       1      0             0**
    MEM_SIZE                  N/A***  N/A*** 16KB          16KB
    PBUF_POOL_SIZE            10      16     24            16
    MEMP_NUM_TCP_SEG          10      16     32            17
    MEMP_NUM_TCP_PCB          5       16     5             dynamic
    MEMP_NUM_TCP_PCB_LISTEN   4       16     8****         dynamic
    MEMP_NUM_UDP_PCB          4       16     7             dynamic
    TCP_SND_QUEUELEN          ~8      17     32            17

    * MEM_LIBC_MALLOC must stay 0: arduino-pico uses
      PICO_CYW43_ARCH_THREADSAFE_BACKGROUND which runs lwIP callbacks from
      a low-priority pendsv IRQ. The pico-sdk explicitly blocks
      MEM_LIBC_MALLOC=1 because libc malloc uses mutexes (unsafe in IRQ).
    ** MEMP_MEM_MALLOC must stay 0: the dedicated lwIP heap (MEM_SIZE=16KB)
      is too small to hold all pools dynamically. The PBUF_POOL alone needs
      ~24KB (16 × 1524 bytes). Increasing MEM_SIZE would negate BSS savings.
    *** ESP8266/ESP32 use MEM_LIBC_MALLOC=1 (system heap, no dedicated pool).
    **** opt.h default; arduino-pico doesn't override MEMP_NUM_TCP_PCB_LISTEN.
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

    # TCP_SND_BUF: 4×MSS=5,840 matches ESP32. Down from arduino-pico's 8×MSS.
    # ESPAsyncWebServer allocates malloc(tcp_sndbuf()) per response chunk.
    tcp_snd_buf = "(4*TCP_MSS)"

    # TCP_WND: receive window. 4×MSS matches ESP32. Down from arduino-pico's 8×MSS.
    tcp_wnd = "(4*TCP_MSS)"

    # TCP_SND_QUEUELEN: max pbufs queued for send buffer
    # ESP-IDF formula: (4 * TCP_SND_BUF + (TCP_MSS - 1)) / TCP_MSS
    # With 4×MSS: (4*5840 + 1459) / 1460 = 17 — match ESP32
    tcp_snd_queuelen = 17
    # MEMP_NUM_TCP_SEG: segment pool, must be >= TCP_SND_QUEUELEN (lwIP sanity check)
    memp_num_tcp_seg = tcp_snd_queuelen

    # PBUF_POOL_SIZE: RP2040 has 264KB RAM, more generous than LibreTiny.
    # 16 matches ESP32 (vs arduino-pico's 24). With MEMP_MEM_MALLOC=1,
    # this is a max count (allocated on demand from heap).
    pbuf_pool_size = 16

    # Build the lwIP override defines for the Jinja2 template.
    # The template uses #include_next to chain to the framework's original
    # lwipopts.h, then #undef/#define only the values we need to change.
    #
    # Note: MEMP_MEM_MALLOC stays 0 (framework default). While the memp
    # allocations use the dedicated lwIP heap (IRQ-safe), the 16KB MEM_SIZE
    # is too small to hold all pools dynamically under stress. The PBUF_POOL
    # alone needs ~24KB (16 × 1524 bytes). Increasing MEM_SIZE would negate
    # the BSS savings.
    #
    # MEM_LIBC_MALLOC stays 0 (framework default): arduino-pico uses
    # PICO_CYW43_ARCH_THREADSAFE_BACKGROUND which runs lwIP callbacks from
    # a low-priority pendsv IRQ where libc malloc (mutex-based) is unsafe.
    lwip_defines: dict[str, str] = {
        "TCP_SND_BUF": tcp_snd_buf,
        "TCP_WND": tcp_wnd,
        "TCP_SND_QUEUELEN": str(tcp_snd_queuelen),
        "MEMP_NUM_TCP_SEG": str(memp_num_tcp_seg),
        "PBUF_POOL_SIZE": str(pbuf_pool_size),
        "MEMP_NUM_TCP_PCB": str(tcp_sockets),
        "MEMP_NUM_TCP_PCB_LISTEN": str(listening_tcp),
        "MEMP_NUM_UDP_PCB": str(udp_sockets),
    }

    # Store for copy_files() to generate the header
    CORE.data[KEY_RP2040][KEY_LWIP_OPTS] = lwip_defines

    # Add a pre-build extra script that injects our lwip_override directory
    # into CCFLAGS so our lwipopts.h shadows the framework's version.
    # Regular build_flags (-I/-isystem) come after -iwithprefixbefore in GCC's
    # search order, so we must prepend via an extra_scripts hook.
    cg.add_platformio_option("extra_scripts", ["pre:inject_lwip_include.py"])

    tcp_min = " (min)" if tcp_sockets > sc.tcp else ""
    udp_min = " (min)" if udp_sockets > sc.udp else ""
    listen_min = " (min)" if listening_tcp > sc.tcp_listen else ""
    _LOGGER.info(
        "Configuring lwIP: TCP=%d%s [%s], UDP=%d%s [%s], TCP_LISTEN=%d%s [%s]",
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
    from jinja2 import Environment, FileSystemLoader

    lwip_defines = CORE.data[KEY_RP2040].get(KEY_LWIP_OPTS)
    if not lwip_defines:
        return

    template_dir = Path(__file__).parent
    jinja_env = Environment(
        loader=FileSystemLoader(str(template_dir)),
        keep_trailing_newline=True,
    )
    template = jinja_env.get_template("lwipopts.h.jinja")
    content = template.render(**lwip_defines)

    lwip_dir = CORE.relative_build_path("lwip_override")
    lwip_dir.mkdir(parents=True, exist_ok=True)
    write_file_if_changed(lwip_dir / "lwipopts.h", content)


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
    inject_lwip_file = dir / "inject_lwip_include.py.script"
    copy_file_if_changed(
        inject_lwip_file,
        CORE.relative_build_path("inject_lwip_include.py"),
    )
    _generate_lwipopts_h()
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
