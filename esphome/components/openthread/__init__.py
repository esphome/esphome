import esphome.codegen as cg
from esphome.core import CORE

# Only import require_vfs_select for ESP32 platform
if CORE.is_esp32:
    from esphome.components.esp32 import require_vfs_select

from esphome.components.esp32 import (
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.mdns import MDNSComponent, enable_mdns_storage
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ENABLE_IPV6, CONF_ID, CONF_USE_ADDRESS
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
    CONF_PSKC,
    CONF_SRP_ID,
    CONF_TLV,
)

CODEOWNERS = ["@mrene"]

AUTO_LOAD = ["network"]

# Wi-fi / Bluetooth / Thread coexistence isn't implemented at this time
# TODO: Doesn't conflict with wifi if you're using another ESP as an RCP (radio coprocessor), but this isn't implemented yet
CONFLICTS_WITH = ["wifi"]
# DEPENDENCIES = ["esp32"]  # Removed - will be platform-specific


CONF_DEVICE_TYPES = [
    "FTD",
    "MTD",
]


def set_esp32_sdkconfig_options(config):
    # and expose options for using SPI/UART RCPs
    add_idf_sdkconfig_option("CONFIG_IEEE802154_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_RADIO_NATIVE", True)

    # There is a conflict if the logger's uart also uses the default UART, which is seen as a watchdog failure on "ot_cli"
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_CLI", False)

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_ENABLED", True)

    if tlv := config.get(CONF_TLV):
        cg.add_define("USE_OPENTHREAD_TLVS", tlv)
    else:
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

    if config.get(CONF_FORCE_DATASET):
        cg.add_define("USE_OPENTHREAD_FORCE_DATASET")

    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_DNS64_CLIENT", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT", True)
    add_idf_sdkconfig_option("CONFIG_OPENTHREAD_SRP_CLIENT_MAX_SERVICES", 5)

    # TODO: Add suport for sleepy end devices
    add_idf_sdkconfig_option(f"CONFIG_OPENTHREAD_{config.get(CONF_DEVICE_TYPE)}", True)


def set_zephyr_config_options(config):
    # Enable OpenThread in Zephyr
    zephyr_add_prj_conf("CONFIG_OPENTHREAD", "y")
    zephyr_add_prj_conf("CONFIG_NET_L2_OPENTHREAD", "y")

    # Enable IEEE 802.15.4 radio
    zephyr_add_prj_conf("CONFIG_IEEE802154", "y")
    zephyr_add_prj_conf("CONFIG_IEEE802154_RAW_MODE", "y")

    # Network configuration
    zephyr_add_prj_conf("CONFIG_NET_IPV6", "y")
    zephyr_add_prj_conf("CONFIG_NET_UDP", "y")
    zephyr_add_prj_conf("CONFIG_NET_SOCKETS", "y")

    # OpenThread features
    zephyr_add_prj_conf("CONFIG_OPENTHREAD_DNS_CLIENT", "y")
    zephyr_add_prj_conf("CONFIG_OPENTHREAD_SRP_CLIENT", "y")

    # Device type configuration
    device_type = config.get(CONF_DEVICE_TYPE)
    if device_type == "FTD":
        zephyr_add_prj_conf("CONFIG_OPENTHREAD_FTD", "y")
    elif device_type == "MTD":
        zephyr_add_prj_conf("CONFIG_OPENTHREAD_MTD", "y")

    # Network credentials - if using TLV, we'll set it in C++ code
    if tlv := config.get(CONF_TLV):
        cg.add_define("USE_OPENTHREAD_TLVS", tlv)
    else:
        # Set individual parameters
        if pan_id := config.get(CONF_PAN_ID):
            zephyr_add_prj_conf("CONFIG_OPENTHREAD_PANID", str(pan_id))

        if channel := config.get(CONF_CHANNEL):
            zephyr_add_prj_conf("CONFIG_OPENTHREAD_CHANNEL", str(channel))

        if network_name := config.get(CONF_NETWORK_NAME):
            zephyr_add_prj_conf("CONFIG_OPENTHREAD_NETWORK_NAME", f'"{network_name}"')

    if config.get(CONF_FORCE_DATASET):
        cg.add_define("USE_OPENTHREAD_FORCE_DATASET")


openthread_ns = cg.esphome_ns.namespace("openthread")
OpenThreadComponent = openthread_ns.class_("OpenThreadComponent", cg.Component)
OpenThreadSrpComponent = openthread_ns.class_("OpenThreadSrpComponent", cg.Component)

_CONNECTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PAN_ID): cv.hex_int,
        cv.Optional(CONF_CHANNEL): cv.int_,
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
    return config


def _require_vfs_select(config):
    """Register VFS select requirement during config validation (ESP32 only)."""
    # OpenThread uses esp_vfs_eventfd which requires VFS select support
    if CORE.is_esp32:
        require_vfs_select()
    return config


def _platform_specific_validation(config):
    """Apply platform-specific validation."""
    if CORE.is_esp32:
        # ESP32 specific validation
        only_on_variant(supported=[VARIANT_ESP32C6, VARIANT_ESP32H2])(config)
        cv.only_with_esp_idf(config)
    elif CORE.is_nrf52:
        # nRF52 uses Zephyr
        cv.only_on_nrf52(config)
    else:
        raise cv.Invalid("OpenThread is only supported on ESP32 and nRF52 platforms")
    return config


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
            cv.Optional(CONF_TLV): cv.string_strict,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
        }
    ).extend(_CONNECTION_SCHEMA),
    cv.has_exactly_one_key(CONF_NETWORK_KEY, CONF_TLV),
    _platform_specific_validation,
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


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    cg.add_define("USE_OPENTHREAD")

    # OpenThread SRP needs access to mDNS services after setup
    enable_mdns_storage()

    ot = cg.new_Pvariable(config[CONF_ID])
    cg.add(ot.set_use_address(config[CONF_USE_ADDRESS]))
    await cg.register_component(ot, config)

    srp = cg.new_Pvariable(config[CONF_SRP_ID])
    mdns_component = await cg.get_variable(config[CONF_MDNS_ID])
    cg.add(srp.set_mdns(mdns_component))
    await cg.register_component(srp, config)

    # Apply platform-specific configuration
    if CORE.is_esp32:
        set_esp32_sdkconfig_options(config)
    elif CORE.is_nrf52:
        set_zephyr_config_options(config)
