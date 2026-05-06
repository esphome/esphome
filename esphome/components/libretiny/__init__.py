import json
import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_COMPONENT_ID,
    CONF_DEBUG,
    CONF_FAMILY,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NAME,
    CONF_OPTIONS,
    CONF_PROJECT,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    ThreadModel,
    __version__,
)
from esphome.core import CORE
from esphome.core.config import BOARD_MAX_LENGTH
from esphome.helpers import copy_file_if_changed
from esphome.storage_json import StorageJSON

from . import gpio  # noqa
from .const import (
    COMPONENT_BK72XX,
    CONF_GPIO_RECOVER,
    CONF_LOGLEVEL,
    CONF_SDK_SILENT,
    CONF_UART_PORT,
    FAMILIES,
    FAMILY_BK7231N,
    FAMILY_BK7238,
    FAMILY_COMPONENT,
    FAMILY_FRIENDLY,
    FAMILY_RTL8710B,
    KEY_BOARD,
    KEY_COMPONENT,
    KEY_COMPONENT_DATA,
    KEY_FAMILY,
    KEY_LIBRETINY,
    LT_DEBUG_MODULES,
    LT_LOGLEVELS,
    LibreTinyComponent,
    LTComponent,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@kuba2k2"]
AUTO_LOAD = ["preferences"]
IS_TARGET_PLATFORM = True

# BLE 5.x BK SDK options to disable unused features.
# Disabling BLE saves ~21KB RAM and ~200KB Flash because BLE init code is
# called unconditionally by the SDK. ESPHome doesn't use BLE on LibreTiny.
#
# This only works on BLE 5.x BK chips (BK7231N, BK7238). Other BK72XX chips
# using BLE 4.2 (BK7231T, BK7231Q, BK7251; BK7252 boards use the BK7251 family)
# have a bug where the BLE library still links and references undefined symbols
# when CFG_SUPPORT_BLE=0.
#
# On BK7238 the SDK also hangs at WiFi STA enable when BLE init runs, so
# disabling it is required for reliable boot, not just an optimization.
#
# Other options like CFG_TX_EVM_TEST, CFG_RX_SENSITIVITY_TEST, CFG_SUPPORT_BKREG,
# CFG_SUPPORT_OTA_HTTP, and CFG_USE_SPI_SLAVE were evaluated but provide no  # NOLINT
# measurable benefit - the linker already strips unreferenced code via -gc-sections.
_BLE5_BK_SYS_CONFIG_OPTIONS = [
    "CFG_SUPPORT_BLE=0",
]


def _detect_variant(value):
    if KEY_LIBRETINY not in CORE.data:
        raise cv.Invalid("Family component didn't populate core data properly!")
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    board = value[CONF_BOARD]
    # read board-default family if not specified
    if board not in component.boards:
        if CONF_FAMILY not in value:
            raise cv.Invalid(
                "This board is unknown, if you are sure you want to compile with this board selection, "
                f"override with option '{CONF_FAMILY}'",
                path=[CONF_BOARD],
            )
        _LOGGER.warning(
            "This board is unknown. Make sure the chosen chip component is correct.",
        )
    else:
        family = component.boards[board][KEY_FAMILY]
        if CONF_FAMILY in value and family != value[CONF_FAMILY]:
            raise cv.Invalid(
                f"Option '{CONF_FAMILY}' does not match selected board.",
                path=[CONF_FAMILY],
            )
        value = value.copy()
        value[CONF_FAMILY] = family
    # read component name matching this family
    value[CONF_COMPONENT_ID] = FAMILY_COMPONENT[value[CONF_FAMILY]]
    # make sure the chosen component matches the family
    if value[CONF_COMPONENT_ID] != component.name:
        raise cv.Invalid(
            f"The chosen family doesn't belong to '{component.name}' component. The correct component is '{value[CONF_COMPONENT_ID]}'",
            path=[CONF_FAMILY],
        )
    return value


def _update_core_data(config):
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = config[CONF_COMPONENT_ID]
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_LIBRETINY][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_LIBRETINY][KEY_COMPONENT] = config[CONF_COMPONENT_ID]
    CORE.data[KEY_LIBRETINY][KEY_FAMILY] = config[CONF_FAMILY]
    return config


def get_libretiny_component(core_obj=None):
    return (core_obj or CORE).data[KEY_LIBRETINY][KEY_COMPONENT]


def get_libretiny_family(core_obj=None):
    return (core_obj or CORE).data[KEY_LIBRETINY][KEY_FAMILY]


def only_on_family(*, supported=None, unsupported=None):
    """Config validator for features only available on some LibreTiny families."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj):
        family = get_libretiny_family()
        if supported is not None and family not in supported:
            raise cv.Invalid(
                f"This feature is only available on {', '.join(supported)}"
            )
        if unsupported is not None and family in unsupported:
            raise cv.Invalid(
                f"This feature is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


def get_download_types(storage_json: StorageJSON = None):
    """Binary-download entries for a built LibreTiny firmware.

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
    types = [
        {
            "title": "UF2 package (recommended)",
            "description": "For flashing via web_server OTA or with ltchiptool (UART)",
            "file": "firmware.uf2",
            "download": f"{storage_json.name}.uf2",
        },
    ]

    build_dir = storage_json.firmware_bin_path.parent
    outputs = build_dir / "firmware.json"
    if not outputs.is_file():
        return types
    with outputs.open(encoding="utf-8") as f:
        outputs = json.load(f)
    for output in outputs:
        if not output["public"]:
            continue
        suffix = output["filename"].partition(".")[2]
        suffix = f"-{suffix}" if "." in suffix else f".{suffix}"
        types.append(
            {
                "title": output["title"],
                "description": output["description"],
                "file": output["filename"],
                "download": storage_json.name + suffix,
            }
        )
    return types


def _notify_old_style(config):
    if config:
        raise cv.Invalid(
            "The LibreTiny component is now split between supported chip families.\n"
            "Migrate your config file to include a chip-based configuration, "
            "instead of the 'libretiny:' block.\n"
            "For example 'bk72xx:' or 'rtl87xx:'."
        )
    return config


# The dev and latest branches will be at *least* this version, which is what matters.
# Use GitHub releases directly to avoid PlatformIO moderation delays.
ARDUINO_VERSIONS = {
    "dev": (cv.Version(1, 12, 1), "https://github.com/libretiny-eu/libretiny.git"),
    "latest": (
        cv.Version(1, 12, 1),
        "https://github.com/libretiny-eu/libretiny.git#v1.12.1",
    ),
    "recommended": (
        cv.Version(1, 12, 1),
        "https://github.com/libretiny-eu/libretiny.git#v1.12.1",
    ),
}


def _check_framework_version(value):
    value = value.copy()

    if value[CONF_VERSION] in ARDUINO_VERSIONS:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = ARDUINO_VERSIONS[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source

    return value


def _check_debug_order(value):
    debug = value[CONF_DEBUG]
    if "NONE" in debug and "NONE" in debug[1:]:
        raise cv.Invalid(
            "'none' has to be specified before other modules, and only once",
            path=[CONF_DEBUG],
        )
    return value


FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_LOGLEVEL, default="warn"): (
                cv.one_of(*LT_LOGLEVELS, upper=True)
            ),
            cv.Optional(CONF_DEBUG, default=[]): cv.ensure_list(
                cv.one_of("NONE", *LT_DEBUG_MODULES, upper=True)
            ),
            cv.Optional(CONF_SDK_SILENT, default="all"): (
                cv.one_of("all", "auto", "none", lower=True)
            ),
            cv.Optional(CONF_UART_PORT): cv.one_of(0, 1, 2, int=True),
            cv.Optional(CONF_GPIO_RECOVER, default=True): cv.boolean,
            cv.Optional(CONF_OPTIONS, default={}): {
                cv.string_strict: cv.string,
            },
        }
    ),
    _check_framework_version,
    _check_debug_order,
)

CONFIG_SCHEMA = cv.All(_notify_old_style)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LTComponent),
        cv.Required(CONF_BOARD): cv.All(
            cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
        ),
        cv.Optional(CONF_FAMILY): cv.one_of(*FAMILIES, upper=True),
        cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
    },
)

BASE_SCHEMA.add_extra(_detect_variant)
BASE_SCHEMA.add_extra(_update_core_data)


def _configure_lwip(config: dict) -> None:
    """Configure lwIP options for LibreTiny platforms.

    The BK/RTL/LN SDKs each ship different lwIP defaults. BK72XX defaults are
    wildly oversized for ESPHome's IoT use case, causing OOM on BK7231N.
    RTL87XX and LN882H have more conservative defaults but still need tuning
    for ESPHome's socket usage patterns.

    See https://github.com/esphome/esphome/issues/14095

    Comparison of SDK defaults vs ESPHome targets (TCP_MSS=1460 on all LT):

    Setting                   ESP8266  ESP32  BK SDK   RTL SDK  LN SDK   New
    ────────────────────────────────────────────────────────────────────────────
    TCP_SND_BUF               2×MSS   4×MSS  10×MSS   5×MSS    7×MSS    4×MSS
    TCP_WND                   4×MSS   4×MSS  3/10×MSS 2×MSS    3×MSS    4×MSS
    MEM_LIBC_MALLOC           1       1      0        0        1        1
    MEMP_MEM_MALLOC           1       1      0        0        0        1
    MEM_SIZE                  N/A*    N/A*   16/32KB  5KB      N/A*     N/A* BK
    PBUF_POOL_SIZE            10      16     3/10     20       20       10 BK
    MAX_SOCKETS_TCP           5       16     12       —**      —**      dynamic
    MAX_SOCKETS_UDP           4       16     22       —**      —**      dynamic
    TCP_SND_QUEUELEN          ~8      17     20       20       35       17
    MEMP_NUM_TCP_SEG          10      16     40       20       =qlen    17
    MEMP_NUM_TCP_PCB          5       16     12       10       8        =TCP
    MEMP_NUM_TCP_PCB_LISTEN   4       16     4        5        3        dynamic
    MEMP_NUM_UDP_PCB          4       16     25***    7****    7****    =UDP
    MEMP_NUM_NETCONN          0       10     38       4*****   =sum     =sum
    MEMP_NUM_NETBUF           0       2      16       2*****   8        4
    MEMP_NUM_TCPIP_MSG_INPKT  4       8      16       8*****   12       8

    * ESP8266/ESP32/LN882H use MEM_LIBC_MALLOC=1 (system heap, no dedicated pool).
      ESP8266/ESP32 also use MEMP_MEM_MALLOC=1 (MEMP pools from heap, not static).
    ** RTL/LN SDKs don't define MAX_SOCKETS_TCP/UDP (LibreTiny-specific).
    *** BK LT overlay: MAX_SOCKETS_UDP+2+1 = 25.
    **** RTL/LN LT overlay overrides to flat 7.
    ***** Not defined in RTL SDK — lwIP opt.h defaults shown.
    "dynamic" = auto-calculated from component socket registrations via
    socket.get_socket_counts() with minimums of 8 TCP / 6 UDP.
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
    # Listening sockets — registered by components (api, ota, web_server_base, etc.)
    # Not all components register yet, so ensure a minimum for baseline operation.
    listening_tcp = max(MIN_TCP_LISTEN_SOCKETS, sc.tcp_listen)

    # TCP_SND_BUF: ESPAsyncWebServer allocates malloc(tcp_sndbuf()) per
    # response chunk. At 10×MSS=14.6KB (BK default) this causes OOM (#14095).
    # 4×MSS=5,840 matches ESP32. RTL(5×) and LN(7×) are close already.
    tcp_snd_buf = "(4*TCP_MSS)"  # BK: 10×MSS, RTL: 5×MSS, LN: 7×MSS

    # TCP_WND: receive window. 4×MSS matches ESP32.
    # RTL SDK uses only 2×MSS; increasing to 4× is safe and improves throughput.
    tcp_wnd = "(4*TCP_MSS)"  # BK: 10×MSS, RTL: 2×MSS, LN: 3×MSS

    # TCP_SND_QUEUELEN: max pbufs queued for send buffer
    # ESP-IDF formula: (4 * TCP_SND_BUF + (TCP_MSS - 1)) / TCP_MSS
    # With 4×MSS: (4*5840 + 1459) / 1460 = 17 — match ESP32
    tcp_snd_queuelen = 17  # BK: 20, RTL: 20, LN: 35
    # MEMP_NUM_TCP_SEG: segment pool, must be >= TCP_SND_QUEUELEN (lwIP sanity check)
    memp_num_tcp_seg = tcp_snd_queuelen  # BK: 40, RTL: 20, LN: =qlen

    lwip_opts: list[str] = [
        # Disable statistics — not needed for production, saves RAM
        "LWIP_STATS=0",  # BK: 1, RTL: 0 already, LN: 0 already
        "MEM_STATS=0",
        "MEMP_STATS=0",
        # TCP send buffer — 4×MSS matches ESP32
        f"TCP_SND_BUF={tcp_snd_buf}",
        # TCP receive window — 4×MSS matches ESP32
        f"TCP_WND={tcp_wnd}",
        # Socket counts — auto-calculated from component registrations
        f"MAX_SOCKETS_TCP={tcp_sockets}",
        f"MAX_SOCKETS_UDP={udp_sockets}",
        # Listening sockets — BK SDK uses this to derive MEMP_NUM_TCP_PCB_LISTEN;
        # RTL/LN don't use it, but we set MEMP_NUM_TCP_PCB_LISTEN explicitly below.
        f"MAX_LISTENING_SOCKETS_TCP={listening_tcp}",
        # Queued segment limits — derived from 4×MSS buffer size
        f"TCP_SND_QUEUELEN={tcp_snd_queuelen}",
        f"MEMP_NUM_TCP_SEG={memp_num_tcp_seg}",  # must be >= queuelen
        # PCB pools — active connections + listening sockets
        f"MEMP_NUM_TCP_PCB={tcp_sockets}",  # BK: 12, RTL: 10, LN: 8
        f"MEMP_NUM_TCP_PCB_LISTEN={listening_tcp}",  # BK: =MAX_LISTENING, RTL: 5, LN: 3
        # UDP PCB pool — includes wifi.lwip_internal (DHCP + DNS)
        f"MEMP_NUM_UDP_PCB={udp_sockets}",  # BK: 25, RTL/LN: 7 via LT
        # Netconn pool — each socket (active + listening) needs a netconn
        f"MEMP_NUM_NETCONN={tcp_sockets + udp_sockets + listening_tcp}",
        # Netbuf pool
        "MEMP_NUM_NETBUF=4",  # BK: 16, RTL: 2 (opt.h), LN: 8
        # Inbound message pool
        "MEMP_NUM_TCPIP_MSG_INPKT=8",  # BK: 16, RTL: 8 (opt.h), LN: 12
    ]

    # Use system heap for all lwIP allocations on all LibreTiny platforms.
    # - MEM_LIBC_MALLOC=1: Use system heap instead of dedicated lwIP heap.
    #   LN882H already ships with this. BK SDK defaults to a 16/32KB dedicated
    #   pool that fragments during OTA. RTL SDK defaults to a 5KB pool.
    #   All three SDKs wire malloc → pvPortMalloc (FreeRTOS thread-safe heap).
    # - MEMP_MEM_MALLOC=1: Allocate MEMP pools from heap on demand instead
    #   of static arrays. Saves ~20KB RAM on BK72XX. Safe because WiFi
    #   receive paths run in task context, not ISR context. ESP32 and ESP8266
    #   both ship with MEMP_MEM_MALLOC=1.
    lwip_opts.append("MEM_LIBC_MALLOC=1")
    lwip_opts.append("MEMP_MEM_MALLOC=1")

    # BK72XX-specific: PBUF_POOL_SIZE override
    # BK SDK "reduced plan" sets this to only 3 — too few for multiple
    # concurrent connections (API + web_server + OTA). BK default plan
    # uses 10; match that. RTL(20) and LN(20) need no override.
    # With MEMP_MEM_MALLOC=1, this is a max count (allocated on demand).
    if CORE.is_bk72xx:
        lwip_opts.append("PBUF_POOL_SIZE=10")

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
    cg.add_platformio_option("custom_options.lwip", lwip_opts)


# pylint: disable=use-dict-literal
async def component_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # setup board config
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_LIBRETINY")
    cg.add_build_flag(f"-DUSE_{config[CONF_COMPONENT_ID].upper()}")
    cg.add_build_flag(f"-DUSE_LIBRETINY_VARIANT_{config[CONF_FAMILY]}")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", FAMILY_FRIENDLY[config[CONF_FAMILY]])
    # Set threading model based on chip architecture
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    if component.supports_atomics:
        # RTL87xx (Cortex-M4) and LN882x (Cortex-M4F) have LDREX/STREX
        cg.add_define(ThreadModel.MULTI_ATOMICS)
    else:
        # BK72xx uses ARM968E-S (ARMv5TE) which lacks LDREX/STREX.
        # std::atomic RMW operations would require libatomic (not linked to save
        # 4-8KB flash). Even if linked, it would use locks, so explicit FreeRTOS
        # mutexes are simpler and equivalent.
        cg.add_define(ThreadModel.MULTI_NO_ATOMICS)
        # Enable FreeRTOS static allocation so FreeRTOSQueue can use
        # xQueueCreateStatic (queue storage in BSS, no heap allocation).
        # Also moves FreeRTOS internal structures (timer command queue) to BSS.
        # BK72xx's FreeRTOSConfig.h doesn't define this, defaulting to 0.
        # The -D wins over the #ifndef default in FreeRTOS.h.
        # Not enabled on RTL87xx/LN882x — costs more heap than it saves there.
        cg.add_build_flag("-DconfigSUPPORT_STATIC_ALLOCATION=1")

    # RTL8710B needs FreeRTOS 8.2.3+ for xTaskNotifyGive/ulTaskNotifyTake
    # required by AsyncTCP 3.4.3+ (https://github.com/esphome/esphome/issues/10220)
    # RTL8720C (ambz2) requires FreeRTOS 10.x so this only applies to RTL8710B
    if config[CONF_FAMILY] == FAMILY_RTL8710B:
        cg.add_platformio_option("custom_versions.freertos", "8.2.3")

    # force using arduino framework
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.set_cpp_standard("gnu++20")

    # disable library compatibility checks
    cg.add_platformio_option("lib_ldf_mode", "off")
    cg.add_platformio_option("lib_compat_mode", "soft")
    # include <Arduino.h> in every file
    build_src_flags = "-include Arduino.h"
    if FAMILY_COMPONENT[config[CONF_FAMILY]] == COMPONENT_BK72XX:
        # LibreTiny forces -O1 globally for BK72xx because the Beken SDK
        # has issues with higher optimization levels. However, ESPHome code
        # works fine with -Os (used on every other platform), so override
        # it for project source files only. GCC uses the last -O flag.
        build_src_flags += " -Os"
    cg.add_platformio_option("build_src_flags", build_src_flags)
    # IRAM_ATTR is a no-op on BK72xx (SDK masks FIQ+IRQ around flash ops).
    # On other families, patch_linker.py routes .sram.text into the right
    # RAM-executable output section and prints a post-link placement summary.
    if FAMILY_COMPONENT[config[CONF_FAMILY]] != COMPONENT_BK72XX:
        cg.add_platformio_option("extra_scripts", ["pre:patch_linker.py"])
    # dummy version code
    cg.add_define("USE_ARDUINO_VERSION_CODE", cg.RawExpression("VERSION_CODE(0, 0, 0)"))
    # decrease web server stack size (16k words -> 4k words)
    cg.add_build_flag("-DCONFIG_ASYNC_TCP_STACK_SIZE=4096")

    # build framework version
    # if platform version is a valid version constraint, prefix the default package
    framework = config[CONF_FRAMEWORK]
    cv.platformio_version_constraint(framework[CONF_VERSION])
    if framework[CONF_SOURCE]:
        cg.add_platformio_option("platform", framework[CONF_SOURCE])
    elif str(framework[CONF_VERSION]) != "0.0.0":
        cg.add_platformio_option("platform", f"libretiny @ {framework[CONF_VERSION]}")
    else:
        cg.add_platformio_option("platform", "libretiny")

    # apply LibreTiny options from framework: block
    # setup LT logger to work nicely with ESPHome logger
    lt_options = dict(
        LT_LOGLEVEL="LT_LEVEL_" + framework[CONF_LOGLEVEL],
        LT_LOGGER_CALLER=0,
        LT_LOGGER_TASK=0,
        LT_LOGGER_COLOR=1,
        LT_USE_TIME=1,
    )
    # enable/disable per-module debugging
    for module in framework[CONF_DEBUG]:
        if module == "NONE":
            # disable all modules
            for module in LT_DEBUG_MODULES:
                lt_options[f"LT_DEBUG_{module}"] = 0
        else:
            # enable one module
            lt_options[f"LT_DEBUG_{module}"] = 1
    # set SDK silencing mode
    if framework[CONF_SDK_SILENT] == "all":
        lt_options["LT_UART_SILENT_ENABLED"] = 1
        lt_options["LT_UART_SILENT_ALL"] = 1
    elif framework[CONF_SDK_SILENT] == "auto":
        lt_options["LT_UART_SILENT_ENABLED"] = 1
        lt_options["LT_UART_SILENT_ALL"] = 0
    else:
        lt_options["LT_UART_SILENT_ENABLED"] = 0
        lt_options["LT_UART_SILENT_ALL"] = 0
    # set default UART port
    if (uart_port := framework.get(CONF_UART_PORT, None)) is not None:
        lt_options["LT_UART_DEFAULT_PORT"] = uart_port
    # add custom options
    lt_options.update(framework[CONF_OPTIONS])

    # apply ESPHome options from framework: block
    cg.add_define("LT_GPIO_RECOVER", int(framework[CONF_GPIO_RECOVER]))

    # build PlatformIO compiler flags
    for name, value in sorted(lt_options.items()):
        cg.add_build_flag(f"-D{name}={value}")

    # custom output firmware name and version
    if CONF_PROJECT in config:
        cg.add_platformio_option(
            "custom_fw_name", "esphome." + config[CONF_PROJECT][CONF_NAME]
        )
        cg.add_platformio_option(
            "custom_fw_version", config[CONF_PROJECT][CONF_VERSION]
        )
    else:
        cg.add_platformio_option("custom_fw_name", "esphome")
        cg.add_platformio_option("custom_fw_version", __version__)

    # Apply chip-specific SDK options to save RAM/Flash
    if config[CONF_FAMILY] in (FAMILY_BK7231N, FAMILY_BK7238):
        cg.add_platformio_option(
            "custom_options.sys_config#h", _BLE5_BK_SYS_CONFIG_OPTIONS
        )

    # Tune lwIP for ESPHome's actual needs.
    # The SDK defaults (TCP_SND_BUF=10*MSS, MAX_SOCKETS_TCP=12, MEM_SIZE=32KB)
    # are wildly oversized for an IoT device. ESPAsyncWebServer allocates
    # malloc(tcp_sndbuf()) per response chunk — at 14.6KB this causes silent
    # OOM on memory-constrained chips like BK7231N.
    # See https://github.com/esphome/esphome/issues/14095
    _configure_lwip(config)

    await cg.register_component(var, config)


# Called by writer.py
def copy_files() -> None:
    script_dir = Path(__file__).parent
    patch_linker_file = script_dir / "patch_linker.py.script"
    copy_file_if_changed(
        patch_linker_file,
        CORE.relative_build_path("patch_linker.py"),
    )
