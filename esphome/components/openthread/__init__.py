from ipaddress import IPv6Network
from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
    get_esp32_variant,
    idf_version,
    include_builtin_idf_component,
    only_on_variant,
    require_vfs_dir,
    require_vfs_select,
)
from esphome.components.mdns import MDNSComponent, enable_mdns_storage
from esphome.components.network import add_use_address
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_ENABLE_IPV6,
    CONF_ENABLE_ON_BOOT,
    CONF_ENABLE_PIN,
    CONF_FRAMEWORK,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_INVERTED,
    CONF_LOG_LEVEL,
    CONF_LOGGER,
    CONF_NETWORKS,
    CONF_NUMBER,
    CONF_OUTPUT_POWER,
    CONF_RESET_PIN,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_USE_ADDRESS,
    PLATFORM_ESP32,
    PlatformFramework,
)
from esphome.core import (
    CORE,
    ID,
    CoroPriority,
    TimePeriodMilliseconds,
    coroutine_with_priority,
)
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import (
    CONF_ANTENNA_SWITCH,
    CONF_BORDER_ROUTER,
    CONF_DEVICE_TYPE,
    CONF_EXT_PAN_ID,
    CONF_EXTERNAL_ANTENNA,
    CONF_FORCE_DATASET,
    CONF_MDNS_ID,
    CONF_MESH_LOCAL_PREFIX,
    CONF_NETWORK_KEY,
    CONF_NETWORK_NAME,
    CONF_PAN_ID,
    CONF_POLL_PERIOD,
    CONF_PSKC,
    CONF_RCP,
    CONF_SELECT_PIN,
    CONF_SRP_ID,
    CONF_TLV,
)

CODEOWNERS = ["@mrene"]

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


def set_sdkconfig_options(config: ConfigType) -> None:
    border_router = CONF_BORDER_ROUTER in config
    rcp = border_router and CONF_RCP in config[CONF_BORDER_ROUTER]

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
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT_MAX_SERVICES", 5)

    if border_router:
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_PLATFORM_NETIF", True)
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_BORDER_ROUTER", True)
        if not rcp:
            add_idf_sdkconfig_option("CONFIG_ESP_COEX_SW_COEXIST_ENABLE", True)

        add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_FORWARD", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_NUM_ADDRESSES", 12)
        add_idf_sdkconfig_option("CONFIG_LWIP_MULTICAST_PING", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_NETIF_STATUS_CALLBACK", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_HOOK_IP6_ROUTE_DEFAULT", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_HOOK_ND6_GET_GW_DEFAULT", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_HOOK_IP6_SELECT_SRC_ADDR_CUSTOM", True)

        add_idf_sdkconfig_option("CONFIG_MDNS_MULTIPLE_INSTANCE", True)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_CMAC_C", True)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_SSL_PROTO_DTLS", True)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_KEY_EXCHANGE_ECJPAKE", True)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_ECJPAKE_C", True)
        add_idf_sdkconfig_option("CONFIG_MDNS_MAX_SERVICES", 50)

    # TODO: Add support for synchronized sleepy end devices (SSED)
    add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)


openthread_ns = cg.esphome_ns.namespace("openthread")
OpenThreadComponent = openthread_ns.class_("OpenThreadComponent", cg.Component)
OpenThreadSrpComponent = openthread_ns.class_("OpenThreadSrpComponent", cg.Component)
OpenThreadBorderRouterComponent = openthread_ns.class_(
    "OpenThreadBorderRouterComponent", cg.Component
)
OpenThreadAntennaSwitchComponent = openthread_ns.class_(
    "OpenThreadAntennaSwitchComponent", cg.Component
)


def _validate_hex_128(value: object) -> int:
    value = cv.hex_int(value)
    if not 0 <= value < 1 << 128:
        raise cv.Invalid("Value must fit in 128 bits")
    return value


def _validate_network_name(value: object) -> str:
    value = cv.string_strict(value)
    length = len(value.encode())
    if not 1 <= length <= 16:
        raise cv.Invalid("Thread network name must be between 1 and 16 bytes")
    return value


def _validate_mesh_local_prefix(value: object) -> IPv6Network:
    value = cv.ipv6network(value)
    if value.prefixlen != 64:
        raise cv.Invalid("Thread mesh local prefix must use a /64 prefix")
    return value


def _validate_antenna_switch(config: ConfigType) -> ConfigType:
    if (enable_pin := config.get(CONF_ENABLE_PIN)) is not None and (
        enable_pin[CONF_NUMBER] == config[CONF_SELECT_PIN][CONF_NUMBER]
    ):
        raise cv.Invalid("Antenna switch enable_pin and select_pin must be different")
    return config


def _validate_antenna_enable_pin(value: object) -> ConfigType:
    value = {CONF_NUMBER: value} if not isinstance(value, dict) else dict(value)
    # Boards with a switch-enable line (e.g. the XIAO ESP32-C6) typically wire it
    # active-low; default to that and let advanced users override it.
    value.setdefault(CONF_INVERTED, True)
    return pins.internal_gpio_output_pin_schema(value)


_ANTENNA_SWITCH_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenThreadAntennaSwitchComponent),
            cv.Optional(CONF_ENABLE_PIN): _validate_antenna_enable_pin,
            cv.Required(CONF_SELECT_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_EXTERNAL_ANTENNA, default=False): cv.boolean,
        }
    ),
    _validate_antenna_switch,
)


def _validate_border_router(value: object) -> ConfigType:
    if value is None:
        value = {}
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenThreadBorderRouterComponent),
            cv.Optional(CONF_RCP): _RCP_SCHEMA,
        }
    )(value)


def _validate_reset_pin(value: object) -> ConfigType:
    value = {CONF_NUMBER: value} if not isinstance(value, dict) else dict(value)
    value.setdefault(CONF_INVERTED, True)
    return pins.internal_gpio_output_pin_schema(value)


def _validate_rcp(config: ConfigType) -> ConfigType:
    pin_numbers = {
        config[CONF_RX_PIN][CONF_NUMBER],
        config[CONF_TX_PIN][CONF_NUMBER],
    }
    if len(pin_numbers) != 2:
        raise cv.Invalid("RCP UART RX and TX pins must be different")
    if (reset_pin := config.get(CONF_RESET_PIN)) is not None and reset_pin[
        CONF_NUMBER
    ] in pin_numbers:
        raise cv.Invalid("RCP reset pin must be different from RX and TX pins")
    return config


_RCP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_BAUD_RATE, default=460800): cv.positive_int,
            cv.Optional(CONF_RESET_PIN): _validate_reset_pin,
        }
    ),
    _validate_rcp,
)


_CONNECTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PAN_ID): cv.hex_int_range(min=0, max=0xFFFE),
        cv.Optional(CONF_CHANNEL): cv.int_range(min=11, max=26),
        cv.Optional(CONF_NETWORK_KEY): cv.sensitive(_validate_hex_128),
        cv.Optional(CONF_EXT_PAN_ID): cv.hex_uint64_t,
        cv.Optional(CONF_NETWORK_NAME): _validate_network_name,
        cv.Optional(CONF_PSKC): cv.sensitive(_validate_hex_128),
        cv.Optional(CONF_MESH_LOCAL_PREFIX): _validate_mesh_local_prefix,
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

    return config


def _require_vfs(config: ConfigType) -> ConfigType:
    """Register VFS requirements during config validation."""
    # OpenThread uses esp_vfs_eventfd which requires VFS select support (ESP32 only)
    if CORE.is_esp32:
        require_vfs_select()
        if (
            border_router := config.get(CONF_BORDER_ROUTER)
        ) is not None and CONF_RCP in border_router:
            require_vfs_dir()
    return config


def _validate_platform(config: ConfigType) -> ConfigType:
    if CORE.using_zephyr:
        return config
    if (
        border_router := config.get(CONF_BORDER_ROUTER)
    ) is not None and CONF_RCP in border_router:
        return cv.only_on([PLATFORM_ESP32])(config)
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


def _validate_tlv_hex(value: object) -> str:
    s = cv.string_strict(value)
    if not s:
        raise cv.Invalid("TLV must not be empty")
    if len(s) % 2 != 0:
        raise cv.Invalid("TLV must have an even number of hex characters")
    if any(char not in "0123456789abcdefABCDEF" for char in s):
        raise cv.Invalid("TLV must contain only hexadecimal characters")
    raw = bytes.fromhex(s)
    if len(raw) > 254:  # sizeof(otOperationalDatasetTlvs::mTlvs)
        raise cv.Invalid(f"TLV too long ({len(raw)} bytes, max 254)")
    return s.lower()


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenThreadComponent),
            cv.GenerateID(CONF_SRP_ID): cv.declare_id(OpenThreadSrpComponent),
            cv.GenerateID(CONF_MDNS_ID): cv.use_id(MDNSComponent),
            cv.Optional(CONF_BORDER_ROUTER): _validate_border_router,
            cv.Optional(CONF_ANTENNA_SWITCH): _ANTENNA_SWITCH_SCHEMA,
            cv.Optional(CONF_DEVICE_TYPE, default="FTD"): cv.one_of(
                *CONF_DEVICE_TYPES, upper=True
            ),
            cv.Optional(CONF_FORCE_DATASET): cv.boolean,
            cv.Optional(CONF_TLV): cv.sensitive(_validate_tlv_hex),
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.Optional(CONF_OUTPUT_POWER): cv.All(
                cv.decibel,
                _validate_txpower,
            ),
            cv.Optional(CONF_POLL_PERIOD): cv.positive_time_period_milliseconds,
        }
    ).extend(_CONNECTION_SCHEMA),
    cv.has_exactly_one_key(CONF_NETWORK_KEY, CONF_TLV),
    _validate_platform,
    _validate,
    _require_vfs,
)


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    network_config = full_config.get("network", {})
    if not network_config.get(CONF_ENABLE_IPV6, False):
        raise cv.Invalid(
            "OpenThread requires IPv6 to be enabled in the network component. "
            "Please set `enable_ipv6: true` in the `network` configuration."
        )

    border_router = CONF_BORDER_ROUTER in config
    wifi_config = full_config.get("wifi")
    if not border_router:
        if wifi_config is not None:
            raise cv.Invalid(
                "OpenThread can only be used with Wi-Fi when 'border_router:' is configured."
            )
    else:
        rcp = config[CONF_BORDER_ROUTER].get(CONF_RCP)
        if not CORE.is_esp32:
            raise cv.Invalid(
                "OpenThread Border Router currently requires an ESP32 with the "
                "ESP-IDF framework."
            )
        if CORE.using_arduino:
            raise cv.Invalid("OpenThread Border Router requires the ESP-IDF framework.")
        if rcp is None and get_esp32_variant() != VARIANT_ESP32C6:
            raise cv.Invalid(
                "OpenThread Border Router with a native radio currently requires ESP32-C6. "
                "Configure 'rcp:' to use an external OpenThread Radio Co-Processor."
            )
        if idf_version() < cv.Version(5, 5, 0):
            raise cv.Invalid(
                "OpenThread Border Router requires ESP-IDF 5.5.0 or newer."
            )
        if config[CONF_DEVICE_TYPE] != "FTD":
            raise cv.Invalid("OpenThread Border Router requires 'device_type: FTD'.")
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
        if rcp is not None:
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
    rcp = config[CONF_BORDER_ROUTER].get(CONF_RCP) if border_router else None

    # Re-enable openthread IDF component (excluded by default)
    if CORE.is_esp32:
        include_builtin_idf_component("openthread")
        if border_router:
            # openthread_esp.cpp uses esp_coexist.h whenever border_router: is configured
            include_builtin_idf_component("esp_coex")

    cg.add_define("USE_OPENTHREAD")
    if border_router:
        cg.add_define("USE_OPENTHREAD_BORDER_ROUTER")
    if rcp is not None:
        cg.add_define("USE_OPENTHREAD_RCP_UART")
    antenna_switch = config.get(CONF_ANTENNA_SWITCH)
    if antenna_switch is not None:
        cg.add_define("USE_OPENTHREAD_ANTENNA_SWITCH")
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
        reset_pin = rcp.get(CONF_RESET_PIN)
        ot = cg.new_Pvariable(
            config[CONF_ID],
            rcp[CONF_BAUD_RATE],
            rcp[CONF_RX_PIN][CONF_NUMBER],
            rcp[CONF_TX_PIN][CONF_NUMBER],
            reset_pin[CONF_NUMBER] if reset_pin is not None else -1,
            not reset_pin[CONF_INVERTED] if reset_pin is not None else False,
        )
    add_use_address(ot, config[CONF_USE_ADDRESS])
    await cg.register_component(ot, config)
    if (poll_period := config.get(CONF_POLL_PERIOD)) is not None:
        cg.add(ot.set_poll_period(poll_period))

    if antenna_switch is not None:
        select_pin = await cg.gpio_pin_expression(antenna_switch[CONF_SELECT_PIN])
        ant_args = [
            antenna_switch[CONF_ID],
            select_pin,
            antenna_switch[CONF_EXTERNAL_ANTENNA],
        ]
        if CONF_ENABLE_PIN in antenna_switch:
            ant_args.append(
                await cg.gpio_pin_expression(antenna_switch[CONF_ENABLE_PIN])
            )
        ant = cg.new_Pvariable(*ant_args)
        await cg.register_component(ant, antenna_switch)

    if border_router:
        border_router_config = config[CONF_BORDER_ROUTER]
        br = cg.new_Pvariable(border_router_config[CONF_ID], ot)
        await cg.register_component(br, border_router_config)
    else:
        mdns_component = await cg.get_variable(config[CONF_MDNS_ID])
        srp = cg.new_Pvariable(config[CONF_SRP_ID])
        cg.add(srp.set_mdns(mdns_component))
        await cg.register_component(srp, config)

    if (output_power := config.get(CONF_OUTPUT_POWER)) is not None:
        cg.add(ot.set_output_power(output_power))

    if CORE.is_esp32:
        set_sdkconfig_options(config)
    elif CORE.using_zephyr:
        zephyr_add_prj_conf("NET_L2_OPENTHREAD", True)
        zephyr_add_prj_conf(
            f"OPENTHREAD_NORDIC_LIBRARY_{config.get(CONF_DEVICE_TYPE)}", True
        )
        zephyr_add_prj_conf(f"OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)
        zephyr_add_prj_conf("MAIN_STACK_SIZE", 4096)


# Actions
OpenThreadComponentPollPeriodAction = openthread_ns.class_(
    "OpenThreadComponentPollPeriodAction",
    automation.Action,
    cg.Parented.template(OpenThreadComponent),
)

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


@automation.register_action(
    "openthread.set_poll_period",
    OpenThreadComponentPollPeriodAction,
    POLL_PERIOD_ACTION_SCHEMA,
    synchronous=True,
)
async def openthread_poll_period_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_POLL_PERIOD], args, cg.uint32)
    cg.add(var.set_poll_period(template_))
    return var
