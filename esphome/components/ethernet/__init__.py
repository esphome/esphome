import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.network import ip_address_literal
from esphome.components.spi import CONF_INTERFACE_INDEX, get_spi_interface
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CLK_PIN,
    CONF_CS_PIN,
    CONF_DNS1,
    CONF_DNS2,
    CONF_DOMAIN,
    CONF_GATEWAY,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_MAC_ADDRESS,
    CONF_MANUAL_IP,
    CONF_MISO_PIN,
    CONF_MODE,
    CONF_MOSI_PIN,
    CONF_NUMBER,
    CONF_PAGE_ID,
    CONF_PIN,
    CONF_POLLING_INTERVAL,
    CONF_RESET_PIN,
    CONF_SPI,
    CONF_STATIC_IP,
    CONF_SUBNET,
    CONF_TYPE,
    CONF_USE_ADDRESS,
    CONF_VALUE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import (
    CORE,
    CoroPriority,
    TimePeriodMilliseconds,
    coroutine_with_priority,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CONFLICTS_WITH = ["wifi"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]
LOGGER = logging.getLogger(__name__)

# RMII pins that are hardcoded on ESP32 classic and cannot be changed
# These pins are used by the internal Ethernet MAC when using RMII PHYs
ESP32_RMII_FIXED_PINS = {
    19: "EMAC_TXD0",
    21: "EMAC_TX_EN",
    22: "EMAC_TXD1",
    25: "EMAC_RXD0",
    26: "EMAC_RXD1",
    27: "EMAC_RX_CRS_DV",
}

# RMII default pins for ESP32-P4
# These are the default pins used by ESP-IDF and are configurable in principle,
# but ESPHome's ethernet component currently has no way to change them
ESP32P4_RMII_DEFAULT_PINS = {
    34: "EMAC_TXD0",
    35: "EMAC_TXD1",
    28: "EMAC_RX_CRS_DV",
    29: "EMAC_RXD0",
    30: "EMAC_RXD1",
    49: "EMAC_TX_EN",
}

ethernet_ns = cg.esphome_ns.namespace("ethernet")
PHYRegister = ethernet_ns.struct("PHYRegister")
CONF_PHY_ADDR = "phy_addr"
CONF_MDC_PIN = "mdc_pin"
CONF_MDIO_PIN = "mdio_pin"
CONF_CLK = "clk"
CONF_CLK_MODE = "clk_mode"
CONF_POWER_PIN = "power_pin"
CONF_PHY_REGISTERS = "phy_registers"

CONF_CLOCK_SPEED = "clock_speed"

EthernetType = ethernet_ns.enum("EthernetType")
ETHERNET_TYPES = {
    "LAN8720": EthernetType.ETHERNET_TYPE_LAN8720,
    "RTL8201": EthernetType.ETHERNET_TYPE_RTL8201,
    "DP83848": EthernetType.ETHERNET_TYPE_DP83848,
    "IP101": EthernetType.ETHERNET_TYPE_IP101,
    "JL1101": EthernetType.ETHERNET_TYPE_JL1101,
    "KSZ8081": EthernetType.ETHERNET_TYPE_KSZ8081,
    "KSZ8081RNA": EthernetType.ETHERNET_TYPE_KSZ8081RNA,
    "W5500": EthernetType.ETHERNET_TYPE_W5500,
    "OPENETH": EthernetType.ETHERNET_TYPE_OPENETH,
    "DM9051": EthernetType.ETHERNET_TYPE_DM9051,
    "LAN8670": EthernetType.ETHERNET_TYPE_LAN8670,
}

# PHY types that need compile-time defines for conditional compilation
_PHY_TYPE_TO_DEFINE = {
    "KSZ8081": "USE_ETHERNET_KSZ8081",
    "KSZ8081RNA": "USE_ETHERNET_KSZ8081",
    "LAN8670": "USE_ETHERNET_LAN8670",
    # Add other PHY types here only if they need conditional compilation
}

SPI_ETHERNET_TYPES = ["W5500", "DM9051"]
SPI_ETHERNET_DEFAULT_POLLING_INTERVAL = TimePeriodMilliseconds(milliseconds=10)

emac_rmii_clock_mode_t = cg.global_ns.enum("emac_rmii_clock_mode_t")

CLK_MODES = {
    "CLK_EXT_IN": emac_rmii_clock_mode_t.EMAC_CLK_EXT_IN,
    "CLK_OUT": emac_rmii_clock_mode_t.EMAC_CLK_OUT,
}

CLK_MODES_DEPRECATED = {
    "GPIO0_IN": ("CLK_EXT_IN", 0),
    "GPIO0_OUT": ("CLK_OUT", 0),
    "GPIO16_OUT": ("CLK_OUT", 16),
    "GPIO17_OUT": ("CLK_OUT", 17),
}

MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4address,
        cv.Required(CONF_GATEWAY): cv.ipv4address,
        cv.Required(CONF_SUBNET): cv.ipv4address,
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4address,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4address,
    }
)

EthernetComponent = ethernet_ns.class_("EthernetComponent", cg.Component)
ManualIP = ethernet_ns.struct("ManualIP")


def _is_framework_spi_polling_mode_supported():
    # SPI Ethernet without IRQ feature is added in
    # esp-idf >= (5.3+ ,5.2.1+, 5.1.4)
    # Note: Arduino now uses ESP-IDF as a component, so we only check IDF version
    framework_version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if framework_version >= cv.Version(5, 3, 0):
        return True
    if cv.Version(5, 3, 0) > framework_version >= cv.Version(5, 2, 1):
        return True
    if cv.Version(5, 2, 0) > framework_version >= cv.Version(5, 1, 4):  # noqa: SIM103
        return True
    return False


def _validate(config):
    if CONF_USE_ADDRESS not in config:
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        else:
            use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address

    if config[CONF_TYPE] in SPI_ETHERNET_TYPES:
        if _is_framework_spi_polling_mode_supported():
            if CONF_POLLING_INTERVAL in config and CONF_INTERRUPT_PIN in config:
                raise cv.Invalid(
                    f"Cannot specify more than one of {CONF_INTERRUPT_PIN}, {CONF_POLLING_INTERVAL}"
                )
            if CONF_POLLING_INTERVAL not in config and CONF_INTERRUPT_PIN not in config:
                config[CONF_POLLING_INTERVAL] = SPI_ETHERNET_DEFAULT_POLLING_INTERVAL
        else:
            if CONF_POLLING_INTERVAL in config:
                raise cv.Invalid(
                    "In this version of the framework "
                    f"({CORE.target_framework} {CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]}), "
                    f"'{CONF_POLLING_INTERVAL}' is not supported."
                )
            if CONF_INTERRUPT_PIN not in config:
                raise cv.Invalid(
                    "In this version of the framework "
                    f"({CORE.target_framework} {CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]}), "
                    f"'{CONF_INTERRUPT_PIN}' is a required option for [ethernet]."
                )
    elif config[CONF_TYPE] != "OPENETH":
        if CONF_CLK_MODE in config:
            LOGGER.warning(
                "[ethernet] The 'clk_mode' option is deprecated and will be removed in ESPHome 2026.1. "
                "Please update your configuration to use 'clk' instead."
            )
            mode = CLK_MODES_DEPRECATED[config[CONF_CLK_MODE]]
            config[CONF_CLK] = CLK_SCHEMA({CONF_MODE: mode[0], CONF_PIN: mode[1]})
            del config[CONF_CLK_MODE]
        elif CONF_CLK not in config:
            raise cv.Invalid("'clk' is a required option for [ethernet].")
        variant = get_esp32_variant()
        if variant not in (VARIANT_ESP32, VARIANT_ESP32P4):
            raise cv.Invalid(
                f"{config[CONF_TYPE]} PHY requires RMII interface and is only supported "
                f"on ESP32 classic and ESP32-P4, not {variant}"
            )

    return config


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EthernetComponent),
        cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
        cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
        cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
        cv.Optional("enable_mdns"): cv.invalid(
            "This option has been removed. Please use the [disabled] option under the "
            "new mdns component instead."
        ),
        cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
    }
).extend(cv.COMPONENT_SCHEMA)

PHY_REGISTER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_int,
        cv.Required(CONF_VALUE): cv.hex_int,
        cv.Optional(CONF_PAGE_ID): cv.hex_int,
    }
)
CLK_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MODE): cv.enum(CLK_MODES, upper=True, space="_"),
        cv.Required(CONF_PIN): pins.internal_gpio_pin_number,
    }
)
RMII_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.Required(CONF_MDC_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MDIO_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLK_MODE): cv.enum(
                CLK_MODES_DEPRECATED, upper=True, space="_"
            ),
            cv.Optional(CONF_CLK): CLK_SCHEMA,
            cv.Optional(CONF_PHY_ADDR, default=0): cv.int_range(min=0, max=31),
            cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_PHY_REGISTERS): cv.ensure_list(PHY_REGISTER_SCHEMA),
        }
    )
)

SPI_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MISO_PIN): pins.internal_gpio_input_pin_number,
            cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLOCK_SPEED, default="26.67MHz"): cv.All(
                cv.frequency, cv.int_range(int(8e6), int(80e6))
            ),
            # Set default value (SPI_ETHERNET_DEFAULT_POLLING_INTERVAL) at _validate()
            cv.Optional(CONF_POLLING_INTERVAL): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriodMilliseconds(milliseconds=1)),
            ),
        }
    ),
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "LAN8720": RMII_SCHEMA,
            "RTL8201": RMII_SCHEMA,
            "DP83848": RMII_SCHEMA,
            "IP101": RMII_SCHEMA,
            "JL1101": RMII_SCHEMA,
            "KSZ8081": RMII_SCHEMA,
            "KSZ8081RNA": RMII_SCHEMA,
            "W5500": SPI_SCHEMA,
            "OPENETH": BASE_SCHEMA,
            "DM9051": SPI_SCHEMA,
            "LAN8670": RMII_SCHEMA,
        },
        upper=True,
    ),
    _validate,
)


def _final_validate_spi(config):
    if config[CONF_TYPE] not in SPI_ETHERNET_TYPES:
        return
    if spi_configs := fv.full_config.get().get(CONF_SPI):
        variant = get_esp32_variant()
        if variant in (
            VARIANT_ESP32C3,
            VARIANT_ESP32C5,
            VARIANT_ESP32C6,
            VARIANT_ESP32C61,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
        ):
            spi_host = "SPI2_HOST"
        else:
            spi_host = "SPI3_HOST"
        for spi_conf in spi_configs:
            if (index := spi_conf.get(CONF_INTERFACE_INDEX)) is not None:
                interface = get_spi_interface(index)
                if interface == spi_host:
                    raise cv.Invalid(
                        f"`spi` component is using interface '{interface}'. "
                        f"To use {config[CONF_TYPE]}, you must change the `interface` on the `spi` component.",
                    )


def manual_ip(config):
    return cg.StructInitializer(
        ManualIP,
        ("static_ip", ip_address_literal(config[CONF_STATIC_IP])),
        ("gateway", ip_address_literal(config[CONF_GATEWAY])),
        ("subnet", ip_address_literal(config[CONF_SUBNET])),
        ("dns1", ip_address_literal(config[CONF_DNS1])),
        ("dns2", ip_address_literal(config[CONF_DNS2])),
    )


def phy_register(address: int, value: int, page: int):
    return cg.StructInitializer(
        PHYRegister,
        ("address", address),
        ("value", value),
        ("page", page),
    )


@coroutine_with_priority(CoroPriority.COMMUNICATION)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_TYPE] in SPI_ETHERNET_TYPES:
        cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
        cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
        cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
        cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
        if CONF_INTERRUPT_PIN in config:
            cg.add(var.set_interrupt_pin(config[CONF_INTERRUPT_PIN]))
        else:
            cg.add(var.set_polling_interval(config[CONF_POLLING_INTERVAL]))
        if _is_framework_spi_polling_mode_supported():
            cg.add_define("USE_ETHERNET_SPI_POLLING_SUPPORT")
        if CONF_RESET_PIN in config:
            cg.add(var.set_reset_pin(config[CONF_RESET_PIN]))
        cg.add(var.set_clock_speed(config[CONF_CLOCK_SPEED]))

        cg.add_define("USE_ETHERNET_SPI")
        add_idf_sdkconfig_option("CONFIG_ETH_USE_SPI_ETHERNET", True)
        add_idf_sdkconfig_option(f"CONFIG_ETH_SPI_ETHERNET_{config[CONF_TYPE]}", True)
    elif config[CONF_TYPE] == "OPENETH":
        cg.add_define("USE_ETHERNET_OPENETH")
        add_idf_sdkconfig_option("CONFIG_ETH_USE_OPENETH", True)
    else:
        cg.add(var.set_phy_addr(config[CONF_PHY_ADDR]))
        cg.add(var.set_mdc_pin(config[CONF_MDC_PIN]))
        cg.add(var.set_mdio_pin(config[CONF_MDIO_PIN]))
        cg.add(var.set_clk_mode(config[CONF_CLK][CONF_MODE]))
        cg.add(var.set_clk_pin(config[CONF_CLK][CONF_PIN]))
        if CONF_POWER_PIN in config:
            cg.add(var.set_power_pin(config[CONF_POWER_PIN]))
        for register_value in config.get(CONF_PHY_REGISTERS, []):
            reg = phy_register(
                register_value.get(CONF_ADDRESS),
                register_value.get(CONF_VALUE),
                register_value.get(CONF_PAGE_ID),
            )
            cg.add(var.add_phy_register(reg))

    cg.add(var.set_type(ETHERNET_TYPES[config[CONF_TYPE]]))
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    if CONF_MANUAL_IP in config:
        cg.add_define("USE_ETHERNET_MANUAL_IP")
        cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    # Add compile-time define for PHY types with specific code
    if phy_define := _PHY_TYPE_TO_DEFINE.get(config[CONF_TYPE]):
        cg.add_define(phy_define)

    if mac_address := config.get(CONF_MAC_ADDRESS):
        cg.add(var.set_fixed_mac(mac_address.parts))

    cg.add_define("USE_ETHERNET")

    # Disable WiFi when using Ethernet to save memory
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_ENABLED", False)
    # Also disable WiFi/BT coexistence since WiFi is disabled
    add_idf_sdkconfig_option("CONFIG_SW_COEXIST_ENABLE", False)

    if config[CONF_TYPE] == "LAN8670":
        # Add LAN867x 10BASE-T1S PHY support component
        add_idf_component(name="espressif/lan867x", ref="2.0.0")

    if CORE.using_arduino:
        cg.add_library("WiFi", None)


def _final_validate_rmii_pins(config: ConfigType) -> None:
    """Validate that RMII pins are not used by other components."""
    # Only validate for RMII-based PHYs on ESP32/ESP32P4
    if config[CONF_TYPE] in SPI_ETHERNET_TYPES or config[CONF_TYPE] == "OPENETH":
        return  # SPI and OPENETH don't use RMII

    variant = get_esp32_variant()
    if variant == VARIANT_ESP32:
        rmii_pins = ESP32_RMII_FIXED_PINS
        is_configurable = False
    elif variant == VARIANT_ESP32P4:
        rmii_pins = ESP32P4_RMII_DEFAULT_PINS
        is_configurable = True
    else:
        return  # No RMII validation needed for other variants

    # Check all used pins against RMII reserved pins
    for pin_list in pins.PIN_SCHEMA_REGISTRY.pins_used.values():
        for pin_path, pin_device, pin_config in pin_list:
            pin_num = pin_config.get(CONF_NUMBER)
            if pin_num not in rmii_pins:
                continue
            # Skip if pin is not directly on ESP, but at some expander (device set to something else than 'None')
            if pin_device is not None:
                continue
            # Found a conflict - show helpful error message
            pin_function = rmii_pins[pin_num]
            component_path = ".".join(str(p) for p in pin_path)
            if is_configurable:
                error_msg = (
                    f"GPIO{pin_num} is used by Ethernet RMII "
                    f"({pin_function}) with the current default "
                    f"configuration. This conflicts with '{component_path}'. "
                    f"Please choose a different GPIO pin for "
                    f"'{component_path}'."
                )
            else:
                error_msg = (
                    f"GPIO{pin_num} is reserved for Ethernet RMII "
                    f"({pin_function}) and cannot be used. This pin is "
                    f"hardcoded by ESP-IDF and cannot be changed when using "
                    f"RMII Ethernet PHYs. Please choose a different GPIO pin "
                    f"for '{component_path}'."
                )
            raise cv.Invalid(error_msg, path=pin_path)


def _final_validate(config: ConfigType) -> ConfigType:
    """Final validation for Ethernet component."""
    _final_validate_spi(config)
    _final_validate_rmii_pins(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate
