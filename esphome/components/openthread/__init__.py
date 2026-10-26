import re
from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import wifi
from esphome.components.const import CONF_BORDER_ROUTER, CONF_DEVICE_TYPE
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
    get_esp32_variant,
    include_builtin_idf_component,
    only_on_variant,
    request_software_coexistence,
    require_mbedtls_ecp,
    require_mbedtls_tls,
    require_mbedtls_tls_extras,
    require_mbedtls_tls_server,
    require_vfs_dir,
    require_vfs_select,
)
from esphome.components.mdns import MDNSComponent, enable_mdns_storage
from esphome.components.network import add_use_address, request_ipv4_off, require_ipv6
from esphome.components.nrf52.framework import include_west_project
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_ENABLE_ON_BOOT,
    CONF_FRAMEWORK,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_INVERTED,
    CONF_LOG_LEVEL,
    CONF_LOGGER,
    CONF_NETWORKS,
    CONF_NUMBER,
    CONF_OPENTHREAD,
    CONF_OUTPUT_POWER,
    CONF_RESET_PIN,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_USE_ADDRESS,
    CONF_WIFI,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    PLATFORM_ESP32,
    PlatformFramework,
)
from esphome.core import (
    CORE,
    CoroPriority,
    TimePeriodMilliseconds,
    coroutine_with_priority,
)
import esphome.final_validate as fv
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType
from esphome.writer import clean_build

from .const import (
    CONF_CORE_OPTIONS,
    CONF_EXT_PAN_ID,
    CONF_FORCE_DATASET,
    CONF_MDNS_ID,
    CONF_MESH_LOCAL_PREFIX,
    CONF_NETWORK_KEY,
    CONF_NETWORK_NAME,
    CONF_PAN_ID,
    CONF_POLL_PERIOD,
    CONF_PSKC,
    CONF_RCP,
    CONF_SRP_ID,
    CONF_TLV,
    OPENTHREAD_CONFIG_PREFIX,
)

CODEOWNERS = ["@mrene"]
DOMAIN = "openthread"

AUTO_LOAD = ["network"]

IDF_TO_OT_LOG_LEVEL = {
    "NONE": "NONE",
    "ERROR": "CRIT",
    "WARN": "WARN",
    "INFO": "NOTE",
    "DEBUG": "INFO",
    "VERBOSE": "DEBG",
}

CONF_DEVICE_TYPES = [
    "FTD",
    "MTD",
]

# Variants with both Wi-Fi and an 802.15.4 radio
NATIVE_BORDER_ROUTER_VARIANTS = {
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
}
# Variants with Wi-Fi, for an external RCP
RCP_BORDER_ROUTER_VARIANTS = {
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
}
# ESP-IDF options a border router needs on top of the plain OpenThread ones
BORDER_ROUTER_SDKCONFIG_OPTIONS: dict[str, bool | int] = {
    "CONFIG_OPENTHREAD_PLATFORM_NETIF": True,
    "CONFIG_OPENTHREAD_BORDER_ROUTER": True,
    # The advertising proxy registers the Thread devices' services with mDNS; the SDK default of 10 fills up fast
    "CONFIG_MDNS_MAX_SERVICES": 20,
    "CONFIG_LWIP_IPV6_FORWARD": True,
    "CONFIG_LWIP_IPV6_NUM_ADDRESSES": 12,
    "CONFIG_LWIP_MULTICAST_PING": True,
    "CONFIG_LWIP_NETIF_STATUS_CALLBACK": True,
    "CONFIG_LWIP_HOOK_IP6_ROUTE_DEFAULT": True,
    "CONFIG_LWIP_HOOK_ND6_GET_GW_DEFAULT": True,
    "CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM": True,
    "CONFIG_LWIP_HOOK_IP6_SELECT_SRC_ADDR_CUSTOM": True,
    "CONFIG_MDNS_MULTIPLE_INSTANCE": True,
    "CONFIG_MBEDTLS_CMAC_C": True,
    "CONFIG_MBEDTLS_SSL_PROTO_DTLS": True,
    "CONFIG_MBEDTLS_KEY_EXCHANGE_ECJPAKE": True,
    "CONFIG_MBEDTLS_ECJPAKE_C": True,
}
OT_HEADER_DIR = "openthread"
OT_HEADER_FILE = "esphome_ot_config.h"
OT_CORE_OPTION_NAME = re.compile(r"[A-Z0-9_]+")


def _validate_txpower(value: Any) -> int | float:
    if CORE.is_esp32:
        variant = get_esp32_variant()

        # HW limits: Datasheet section "802.15.4 RF Transmitter (TX) Characteristics"
        # Further regulatory/soft limit may apply, e.g. by region
        if variant in (VARIANT_ESP32C6, VARIANT_ESP32C5):
            return cv.int_range(min=-15, max=20)(value)
        if variant == VARIANT_ESP32H2:
            return cv.int_range(min=-24, max=20)(value)

    return value  # Unsupported, fail later with clear error


def _get_rcp(config: ConfigType) -> ConfigType | None:
    return config.get(CONF_BORDER_ROUTER, {}).get(CONF_RCP)


def set_sdkconfig_options(config: ConfigType) -> None:
    border_router = CONF_BORDER_ROUTER in config
    rcp = _get_rcp(config) is not None

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_RADIO_NATIVE", not rcp)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_RADIO_SPINEL_UART", rcp)
    if not rcp:
        add_idf_sdkconfig_option("CONFIG_IEEE802154_ENABLED", True)

    # There is a conflict if the logger's uart also uses the default UART, which is seen as a watchdog failure on "ot_cli"
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CLI", False)
    # Console is the transport layer for CLI; disable it too since CLI is disabled
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CONSOLE_ENABLE", False)

    # Diag unused, if needed for lab/cert/etc tests then enable separately
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_DIAG", False)

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_ENABLED", True)

    # OpenThread's SRP and border-router code uses these mbedTLS crypto features directly.
    require_mbedtls_tls_extras(
        ("CONFIG_MBEDTLS_CCM_C", "CONFIG_MBEDTLS_ECDSA_DETERMINISTIC")
    )
    # Widens the TLS role only where TLS is compiled in: a commissioner or border agent enabled
    # through sdkconfig_options needs the DTLS server side too
    require_mbedtls_tls_server()
    if border_router:
        require_mbedtls_tls()

    if not config.get(CONF_TLV):
        if (pan_id := config.get(CONF_PAN_ID)) is not None:
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_PANID", pan_id)

        if channel := config.get(CONF_CHANNEL):
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_CHANNEL", channel)

        if (network_key := config.get(CONF_NETWORK_KEY)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_NETWORK_MASTERKEY", f"{network_key:032x}"
            )

        if (network_name := config.get(CONF_NETWORK_NAME)) is not None:
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_NAME", network_name)

        if (ext_pan_id := config.get(CONF_EXT_PAN_ID)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_NETWORK_EXTPANID", f"{ext_pan_id:016x}"
            )
        if (mesh_local_prefix := config.get(CONF_MESH_LOCAL_PREFIX)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_MESH_LOCAL_PREFIX", f"{mesh_local_prefix}".lower()
            )
        if (pskc := config.get(CONF_PSKC)) is not None:
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_PSKC", f"{pskc:032x}")

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_DNS64_CLIENT", not border_router)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT", not border_router)
    if not border_router:
        # The DNS64 hook breaks IPv6 literal lookups; see openthread_esp.cpp.
        # Fixed upstream on ESP-IDF master, remove this once a release includes
        # "fix(openthread): bypass DNS64 synthesis for IPv6 literals" (merged in
        # https://github.com/espressif/esp-idf/commit/83239509832bf5d427935cd365f4b62a320e8765)
        cg.add_build_flag("-Wl,--wrap=lwip_hook_dns_external_resolve")
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT_MAX_SERVICES", 5)

    if border_router:
        # Border routing follows the Wi-Fi link
        wifi.request_wifi_connect_state_listener()
        for name, value in BORDER_ROUTER_SDKCONFIG_OPTIONS.items():
            add_idf_sdkconfig_option(name, value)
        if not rcp:
            # The native 802.15.4 radio shares the RF frontend with Wi-Fi
            request_software_coexistence()

    # TODO: Add support for synchronized sleepy end devices (SSED)
    add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)


def _write_custom_header(config: ConfigType) -> None:
    """Write core_options to the OpenThread custom header."""
    if not (options := config.get(CONF_CORE_OPTIONS)):
        return

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_HEADER_CUSTOM", True)
    # IDF resolves the path relative to the project (build) directory.
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CUSTOM_HEADER_PATH", OT_HEADER_DIR)
    add_idf_sdkconfig_option(
        "CONFIG_OPENTHREAD_CUSTOM_HEADER_FILE_NAME", OT_HEADER_FILE
    )
    content = (
        "// Auto-generated by ESPHome from openthread core_options - do not edit\n"
        "#pragma once\n\n"
        + "".join(
            f"#define {name} {value}\n" for name, value in sorted(options.items())
        )
    )
    # PlatformIO does not track this header as an OpenThread input; the esp-idf
    # toolchain does.
    header_path = CORE.relative_build_path(OT_HEADER_DIR, OT_HEADER_FILE)
    if write_file_if_changed(header_path, content) and not CORE.using_toolchain_esp_idf:
        clean_build(clear_pio_cache=False)


openthread_ns = cg.esphome_ns.namespace("openthread")
OpenThreadComponent = openthread_ns.class_("OpenThreadComponent", cg.Component)
OpenThreadSrpComponent = openthread_ns.class_("OpenThreadSrpComponent", cg.Component)


def _validate_reset_pin(value: object) -> ConfigType:
    value = {CONF_NUMBER: value} if not isinstance(value, dict) else dict(value)
    value.setdefault(CONF_INVERTED, True)
    return pins.internal_gpio_output_pin_schema(value)


# The pin registry rejects a pin used twice, so RX, TX and reset need no check of their own
_RCP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_BAUD_RATE, default=460800): cv.int_range(min=9600),
        cv.Optional(CONF_RESET_PIN): _validate_reset_pin,
    }
)

_BORDER_ROUTER_SCHEMA = cv.Schema({cv.Optional(CONF_RCP): _RCP_SCHEMA})


def _validate_border_router(value: object) -> ConfigType:
    return _BORDER_ROUTER_SCHEMA({} if value is None else value)


_CONNECTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PAN_ID): cv.hex_int,
        cv.Optional(CONF_CHANNEL): cv.int_range(min=11, max=26),
        cv.Optional(CONF_NETWORK_KEY): cv.sensitive(cv.hex_int),
        cv.Optional(CONF_EXT_PAN_ID): cv.hex_int,
        cv.Optional(CONF_NETWORK_NAME): cv.string_strict,
        cv.Optional(CONF_PSKC): cv.sensitive(cv.hex_int),
        cv.Optional(CONF_MESH_LOCAL_PREFIX): cv.ipv6network,
    }
)


def _validate(config: ConfigType) -> ConfigType:
    if CONF_USE_ADDRESS not in config:
        config[CONF_USE_ADDRESS] = f"{CORE.name}.local"
    if CORE.using_zephyr and CONF_TLV not in config:
        raise cv.Invalid(
            "On nRF52, OpenThread credentials must be provided via 'tlv'. "
            "Individual parameters (network_key, pan_id, channel, etc.) are not yet supported on this platform."
        )
    device_type = config.get(CONF_DEVICE_TYPE)
    poll_period = config.get(CONF_POLL_PERIOD)
    if (
        device_type == "FTD"
        and poll_period
        and poll_period > TimePeriodMilliseconds(milliseconds=0)
    ):
        raise cv.Invalid(
            f"{CONF_POLL_PERIOD} can only be used with {CONF_DEVICE_TYPE}: MTD"
        )
    if CONF_BORDER_ROUTER in config and device_type != "FTD":
        raise cv.Invalid("OpenThread Border Router requires 'device_type: FTD'.")

    return config


def _require_vfs(config: ConfigType) -> ConfigType:
    """Register VFS requirements during config validation."""
    # OpenThread uses esp_vfs_eventfd which requires VFS select support (ESP32 only)
    if CORE.is_esp32:
        require_vfs_select()
        if _get_rcp(config) is not None:
            require_vfs_dir()
    return config


def _validate_platform(config: ConfigType) -> ConfigType:
    if CORE.using_zephyr:
        return config
    # Older 5.5 releases keep the first lock mutex when a timed wait for the second one runs out
    cv.require_framework_version(
        esp_idf=cv.Version(5, 5, 5),
        extra_message="OpenThread requires ESP-IDF 5.5.5 or newer; older 5.5 releases can deadlock the OpenThread lock",
    )(config)
    if CONF_BORDER_ROUTER in config:
        variant = get_esp32_variant()
        if _get_rcp(config) is not None:
            if variant not in RCP_BORDER_ROUTER_VARIANTS:
                raise cv.Invalid(
                    "OpenThread Border Router RCP mode requires a Wi-Fi-capable ESP32 variant."
                )
        elif variant not in NATIVE_BORDER_ROUTER_VARIANTS:
            raise cv.Invalid(
                "OpenThread Border Router with a native radio requires an ESP32 variant "
                "with both Wi-Fi and IEEE 802.15.4 support. "
                "Configure 'rcp:' to use an external OpenThread Radio Co-Processor."
            )
        return config
    return only_on_variant(
        supported=[
            VARIANT_ESP32C5,
            VARIANT_ESP32C6,
            VARIANT_ESP32H2,
            VARIANT_ESP32H4,
            VARIANT_ESP32H21,
            VARIANT_ESP32S31,
        ]
    )(config)


def _validate_core_option_names(options: dict[str, str]) -> dict[str, str]:
    for name in options:
        if (
            not name.startswith(OPENTHREAD_CONFIG_PREFIX)
            or name == OPENTHREAD_CONFIG_PREFIX
        ):
            raise cv.Invalid(
                f"'{name}' is not an OpenThread core option; names must be "
                f"{OPENTHREAD_CONFIG_PREFIX} followed by the option name",
                path=[name],
            )
        if not OT_CORE_OPTION_NAME.fullmatch(name):
            raise cv.Invalid(f"'{name}' may only contain A-Z, 0-9 and _", path=[name])
    return options


def _validate_core_option_value(value: Any) -> str:
    value = cv.string_strict(value)
    if not value.strip():
        raise cv.Invalid("OpenThread core option value must not be empty")
    if "\n" in value or "\r" in value:
        raise cv.Invalid("OpenThread core option value must be a single line")
    # GCC treats a backslash followed by spaces as a line continuation too.
    if value.rstrip().endswith("\\"):
        raise cv.Invalid("OpenThread core option value must not end with \\")
    return value


def _validate_tlv_hex(value: Any) -> str:
    s = cv.string_strict(value)
    if len(s) % 2 != 0:
        raise cv.Invalid("TLV must have an even number of hex characters")
    try:
        raw = bytes.fromhex(s)
    except ValueError as err:
        raise cv.Invalid(f"TLV must be valid hex: {err}") from err
    if len(raw) > 254:  # sizeof(otOperationalDatasetTlvs::mTlvs)
        raise cv.Invalid(f"TLV too long ({len(raw)} bytes, max 254)")
    return s


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenThreadComponent),
            cv.GenerateID(CONF_SRP_ID): cv.declare_id(OpenThreadSrpComponent),
            cv.GenerateID(CONF_MDNS_ID): cv.use_id(MDNSComponent),
            cv.Optional(CONF_BORDER_ROUTER): _validate_border_router,
            cv.Optional(CONF_DEVICE_TYPE, default="FTD"): cv.one_of(
                *CONF_DEVICE_TYPES, upper=True
            ),
            cv.Optional(CONF_FORCE_DATASET): cv.boolean,
            cv.Optional(CONF_TLV): cv.sensitive(
                cv.All(cv.string_strict, _validate_tlv_hex)
            ),
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.Optional(CONF_OUTPUT_POWER): cv.All(
                cv.decibel,
                _validate_txpower,
            ),
            cv.Optional(CONF_POLL_PERIOD): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CORE_OPTIONS): cv.All(
                cv.only_on_esp32,
                {cv.string_strict: _validate_core_option_value},
                _validate_core_option_names,
            ),
        }
    ).extend(_CONNECTION_SCHEMA),
    cv.has_exactly_one_key(CONF_NETWORK_KEY, CONF_TLV),
    _validate_platform,
    _validate,
    _require_vfs,
    require_ipv6(CONF_OPENTHREAD),
    request_ipv4_off(CONF_OPENTHREAD),
)


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()

    wifi_config = full_config.get(CONF_WIFI)
    if CONF_BORDER_ROUTER not in config:
        if wifi_config is not None:
            raise cv.Invalid(
                "OpenThread can only be used with Wi-Fi when 'border_router:' is configured."
            )
    else:
        if wifi_config is None:
            raise cv.Invalid("OpenThread Border Router requires a Wi-Fi STA backbone.")
        if CONF_AP in wifi_config:
            raise cv.Invalid(
                "OpenThread Border Router does not support Wi-Fi AP or fallback AP mode."
            )
        if not wifi_config.get(CONF_NETWORKS):
            raise cv.Invalid(
                "OpenThread Border Router requires at least one configured Wi-Fi STA network."
            )
        if not wifi_config[CONF_ENABLE_ON_BOOT]:
            raise cv.Invalid(
                "OpenThread Border Router requires Wi-Fi 'enable_on_boot: true'."
            )
        if _get_rcp(config) is not None:
            if full_config.get("uart") is not None:
                raise cv.Invalid(
                    "OpenThread RCP uses ESP-IDF's dedicated Spinel UART driver and "
                    "cannot currently be combined with the ESPHome UART component."
                )
            if (
                (logger_config := full_config.get(CONF_LOGGER)) is not None
                and logger_config.get(CONF_HARDWARE_UART) == "UART1"
                and logger_config.get(CONF_BAUD_RATE, 0) > 0
            ):
                raise cv.Invalid(
                    "OpenThread RCP reserves UART1; configure the logger to use USB "
                    "or another UART."
                )

    if (
        (esp32_config := full_config.get(PLATFORM_ESP32)) is not None
        and (fw_config := esp32_config.get(CONF_FRAMEWORK)) is not None
        and (log_level := fw_config.get(CONF_LOG_LEVEL)) is not None
    ):
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC", False)
        ot_log_level = IDF_TO_OT_LOG_LEVEL.get(log_level, log_level)
        add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_LOG_LEVEL_{ot_log_level}", True)

    return config


FINAL_VALIDATE_SCHEMA = _final_validate

FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "openthread_esp.cpp": {
            PlatformFramework.ESP32_IDF,
        },
        "openthread_zephyr.cpp": {PlatformFramework.NRF52_ZEPHYR},
    }
)


@coroutine_with_priority(CoroPriority.COMMUNICATION)
async def to_code(config: ConfigType) -> None:
    border_router = CONF_BORDER_ROUTER in config
    rcp = _get_rcp(config)

    # Re-enable openthread IDF component (excluded by default)
    if CORE.is_esp32:
        include_builtin_idf_component("openthread")
        # OPENTHREAD_CONFIG_ECDSA_ENABLE: the SRP client host key uses mbedtls_ecdsa_*
        require_mbedtls_ecp()

    cg.add_define("USE_OPENTHREAD")
    if border_router:
        cg.add_define("USE_OPENTHREAD_BORDER_ROUTER")
    else:
        # The SRP client registers the mDNS services with the Thread network
        cg.add_define("USE_OPENTHREAD_SRP")
    if rcp is not None:
        cg.add_define("USE_OPENTHREAD_RCP_UART")
    if config.get(CONF_FORCE_DATASET):
        cg.add_define("USE_OPENTHREAD_FORCE_DATASET")
    if tlv := config.get(CONF_TLV):
        cg.add_define("USE_OPENTHREAD_TLVS", tlv)

    if not border_router:
        # OpenThread SRP needs access to mDNS services after setup
        enable_mdns_storage()

    if rcp is None:
        ot = cg.new_Pvariable(config[CONF_ID])
    else:
        reset_pin = (
            await cg.gpio_pin_expression(rcp[CONF_RESET_PIN])
            if CONF_RESET_PIN in rcp
            else cg.nullptr
        )
        ot = cg.new_Pvariable(
            config[CONF_ID],
            rcp[CONF_BAUD_RATE],
            rcp[CONF_RX_PIN],
            rcp[CONF_TX_PIN],
            reset_pin,
        )
    add_use_address(ot, config[CONF_USE_ADDRESS])
    await cg.register_component(ot, config)
    cg.add_optional(ot.set_poll_period, config.get(CONF_POLL_PERIOD))

    if not border_router:
        mdns_component = await cg.get_variable(config[CONF_MDNS_ID])
        srp = cg.new_Pvariable(config[CONF_SRP_ID])
        cg.add(srp.set_mdns(mdns_component))
        await cg.register_component(srp, config)

    cg.add_optional(ot.set_output_power, config.get(CONF_OUTPUT_POWER))

    if CORE.is_esp32:
        set_sdkconfig_options(config)
        _write_custom_header(config)
    elif CORE.using_zephyr:
        # Crypto through PSA: mbedtls, plus Oberon from SDK 2.7
        include_west_project("mbedtls")
        include_west_project("openthread")
        if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(2, 7, 0):
            include_west_project("oberon-psa-crypto")
        zephyr_add_prj_conf("NET_L2_OPENTHREAD", True)
        zephyr_add_prj_conf(
            f"OPENTHREAD_NORDIC_LIBRARY_{config.get(CONF_DEVICE_TYPE)}", True
        )
        zephyr_add_prj_conf(f"OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)
        zephyr_add_prj_conf("MAIN_STACK_SIZE", 4096)


# Actions
POLL_PERIOD_ACTION_SCHEMA = automation.maybe_conf(
    CONF_POLL_PERIOD,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(OpenThreadComponent),
            cv.Required(CONF_POLL_PERIOD): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
)


automation.register_apply_action(
    "openthread.set_poll_period",
    POLL_PERIOD_ACTION_SCHEMA,
    automation.ApplyField(CONF_POLL_PERIOD, "apply_poll_period", cg.uint32),
)
