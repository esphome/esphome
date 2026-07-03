from esphome import automation
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
    include_builtin_idf_component,
    only_on_variant,
    require_vfs_select,
)
from esphome.components.mdns import MDNSComponent, enable_mdns_storage
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ENABLE_IPV6,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_LOG_LEVEL,
    CONF_OUTPUT_POWER,
    CONF_USE_ADDRESS,
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
from esphome.types import ConfigType

from .const import (
    CONF_DEVICE_TYPE,
    CONF_EXT_PAN_ID,
    CONF_FORCE_DATASET,
    CONF_MDNS_ID,
    CONF_MESH_LOCAL_PREFIX,
    CONF_NETWORK_KEY,
    CONF_NETWORK_NAME,
    CONF_PAN_ID,
    CONF_POLL_PERIOD,
    CONF_PSKC,
    CONF_SRP_ID,
    CONF_TLV,
)

CODEOWNERS = ["@mrene"]

AUTO_LOAD = ["network"]

# Wi-fi / Bluetooth / Thread coexistence isn't implemented at this time
# TODO: Doesn't conflict with wifi if you're using another ESP as an RCP (radio coprocessor), but this isn't implemented yet
CONFLICTS_WITH = ["wifi"]

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


def _validate_txpower(value):
    if CORE.is_esp32:
        variant = get_esp32_variant()

        # HW limits: Datasheet section "802.15.4 RF Transmitter (TX) Characteristics"
        # Further regulatory/soft limit may apply, e.g. by region
        if variant in (VARIANT_ESP32C6, VARIANT_ESP32C5):
            return cv.int_range(min=-15, max=20)(value)
        if variant == VARIANT_ESP32H2:
            return cv.int_range(min=-24, max=20)(value)

    return value  # Unsupported, fail later with clear error


def set_sdkconfig_options(config):
    # and expose options for using SPI/UART RCPs
    add_idf_sdkconfig_option("CONFIG_IEEE802154_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_RADIO_NATIVE", True)

    # There is a conflict if the logger's uart also uses the default UART, which is seen as a watchdog failure on "ot_cli"
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CLI", False)
    # Console is the transport layer for CLI; disable it too since CLI is disabled
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CONSOLE_ENABLE", False)

    # Diag unused, if needed for lab/cert/etc tests then enable separately
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_DIAG", False)

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_ENABLED", True)

    if not config.get(CONF_TLV):
        if pan_id := config.get(CONF_PAN_ID):
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_PANID", pan_id)

        if channel := config.get(CONF_CHANNEL):
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_CHANNEL", channel)

        if network_key := config.get(CONF_NETWORK_KEY):
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_NETWORK_MASTERKEY", f"{network_key:X}".lower()
            )

        if network_name := config.get(CONF_NETWORK_NAME):
            add_idf_sdkconfig_option("CONFIG_OPENTHREAD_NETWORK_NAME", network_name)

        if (ext_pan_id := config.get(CONF_EXT_PAN_ID)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_NETWORK_EXTPANID", f"{ext_pan_id:X}".lower()
            )
        if (mesh_local_prefix := config.get(CONF_MESH_LOCAL_PREFIX)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_MESH_LOCAL_PREFIX", f"{mesh_local_prefix}".lower()
            )
        if (pskc := config.get(CONF_PSKC)) is not None:
            add_idf_sdkconfig_option(
                "CONFIG_OPENTHREAD_NETWORK_PSKC", f"{pskc:X}".lower()
            )

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_DNS64_CLIENT", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT_MAX_SERVICES", 5)

    # TODO: Add support for synchronized sleepy end devices (SSED)
    add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)


openthread_ns = cg.esphome_ns.namespace("openthread")
OpenThreadComponent = openthread_ns.class_("OpenThreadComponent", cg.Component)
OpenThreadSrpComponent = openthread_ns.class_("OpenThreadSrpComponent", cg.Component)

_CONNECTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PAN_ID): cv.hex_int,
        cv.Optional(CONF_CHANNEL): cv.int_range(min=11, max=26),
        cv.Optional(CONF_NETWORK_KEY): cv.hex_int,
        cv.Optional(CONF_EXT_PAN_ID): cv.hex_int,
        cv.Optional(CONF_NETWORK_NAME): cv.string_strict,
        cv.Optional(CONF_PSKC): cv.hex_int,
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

    return config


def _require_vfs_select(config):
    """Register VFS select requirement during config validation."""
    # OpenThread uses esp_vfs_eventfd which requires VFS select support (ESP32 only)
    if CORE.is_esp32:
        require_vfs_select()
    return config


def _validate_platform(config):
    if CORE.using_zephyr:
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


def _validate_tlv_hex(value):
    s = cv.string_strict(value)
    if len(s) % 2 != 0:
        raise cv.Invalid("TLV must have an even number of hex characters")
    try:
        raw = bytes.fromhex(s)
    except ValueError as e:
        raise cv.Invalid(f"TLV must be valid hex: {e}") from e
    if len(raw) > 254:  # sizeof(otOperationalDatasetTlvs::mTlvs)
        raise cv.Invalid(f"TLV too long ({len(raw)} bytes, max 254)")
    return s


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenThreadComponent),
            cv.GenerateID(CONF_SRP_ID): cv.declare_id(OpenThreadSrpComponent),
            cv.GenerateID(CONF_MDNS_ID): cv.use_id(MDNSComponent),
            cv.Optional(CONF_DEVICE_TYPE, default="FTD"): cv.one_of(
                *CONF_DEVICE_TYPES, upper=True
            ),
            cv.Optional(CONF_FORCE_DATASET): cv.boolean,
            cv.Optional(CONF_TLV): cv.All(cv.string_strict, _validate_tlv_hex),
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
    _require_vfs_select,
)


def _final_validate(_):
    full_config = fv.full_config.get()
    network_config = full_config.get("network", {})
    if not network_config.get(CONF_ENABLE_IPV6, False):
        raise cv.Invalid(
            "OpenThread requires IPv6 to be enabled in the network component. "
            "Please set `enable_ipv6: true` in the `network` configuration."
        )

    if (
        (esp32_config := full_config.get(PLATFORM_ESP32)) is not None
        and (fw_config := esp32_config.get(CONF_FRAMEWORK)) is not None
        and (log_level := fw_config.get(CONF_LOG_LEVEL)) is not None
    ):
        add_idf_sdkconfig_option("CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC", False)
        ot_log_level = IDF_TO_OT_LOG_LEVEL.get(log_level, log_level)
        add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_LOG_LEVEL_{ot_log_level}", True)


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
async def to_code(config):
    # Re-enable openthread IDF component (excluded by default)
    if CORE.is_esp32:
        include_builtin_idf_component("openthread")

    cg.add_define("USE_OPENTHREAD")
    if config.get(CONF_FORCE_DATASET):
        cg.add_define("USE_OPENTHREAD_FORCE_DATASET")
    if tlv := config.get(CONF_TLV):
        cg.add_define("USE_OPENTHREAD_TLVS", tlv)

    # OpenThread SRP needs access to mDNS services after setup
    enable_mdns_storage()

    ot = cg.new_Pvariable(config[CONF_ID])
    cg.add(ot.set_use_address(config[CONF_USE_ADDRESS]))
    await cg.register_component(ot, config)
    if (poll_period := config.get(CONF_POLL_PERIOD)) is not None:
        cg.add(ot.set_poll_period(poll_period))

    srp = cg.new_Pvariable(config[CONF_SRP_ID])
    mdns_component = await cg.get_variable(config[CONF_MDNS_ID])
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
async def openthread_poll_period_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_POLL_PERIOD], args, cg.uint32)
    cg.add(var.set_poll_period(template_))
    return var
