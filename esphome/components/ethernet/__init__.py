from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option, get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
from esphome.components.network import IPAddress
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
    CONF_MANUAL_IP,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_PAGE_ID,
    CONF_RESET_PIN,
    CONF_SPI,
    CONF_STATIC_IP,
    CONF_SUBNET,
    CONF_TYPE,
    CONF_USE_ADDRESS,
    CONF_VALUE,
)
from esphome.core import CORE, coroutine_with_priority
import esphome.final_validate as fv

CONFLICTS_WITH = ["wifi"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]

ethernet_ns = cg.esphome_ns.namespace("ethernet")
PHYRegister = ethernet_ns.struct("PHYRegister")
CONF_PHY_ADDR = "phy_addr"
CONF_MDC_PIN = "mdc_pin"
CONF_MDIO_PIN = "mdio_pin"
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
}

SPI_ETHERNET_TYPES = ["W5500"]

emac_rmii_clock_mode_t = cg.global_ns.enum("emac_rmii_clock_mode_t")
emac_rmii_clock_gpio_t = cg.global_ns.enum("emac_rmii_clock_gpio_t")
CLK_MODES = {
    "GPIO0_IN": (
        emac_rmii_clock_mode_t.EMAC_CLK_EXT_IN,
        emac_rmii_clock_gpio_t.EMAC_CLK_IN_GPIO,
    ),
    "GPIO0_OUT": (
        emac_rmii_clock_mode_t.EMAC_CLK_OUT,
        emac_rmii_clock_gpio_t.EMAC_APPL_CLK_OUT_GPIO,
    ),
    "GPIO16_OUT": (
        emac_rmii_clock_mode_t.EMAC_CLK_OUT,
        emac_rmii_clock_gpio_t.EMAC_CLK_OUT_GPIO,
    ),
    "GPIO17_OUT": (
        emac_rmii_clock_mode_t.EMAC_CLK_OUT,
        emac_rmii_clock_gpio_t.EMAC_CLK_OUT_180_GPIO,
    ),
}


MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4,
        cv.Required(CONF_GATEWAY): cv.ipv4,
        cv.Required(CONF_SUBNET): cv.ipv4,
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4,
    }
)

EthernetComponent = ethernet_ns.class_("EthernetComponent", cg.Component)
ManualIP = ethernet_ns.struct("ManualIP")


def _validate(config):
    if CONF_USE_ADDRESS not in config:
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        else:
            use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address
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
    }
).extend(cv.COMPONENT_SCHEMA)

PHY_REGISTER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_int,
        cv.Required(CONF_VALUE): cv.hex_int,
        cv.Optional(CONF_PAGE_ID): cv.hex_int,
    }
)
RMII_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.Required(CONF_MDC_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MDIO_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLK_MODE, default="GPIO0_IN"): cv.enum(
                CLK_MODES, upper=True, space="_"
            ),
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
        },
        upper=True,
    ),
    _validate,
)


def _final_validate(config):
    if config[CONF_TYPE] not in SPI_ETHERNET_TYPES:
        return
    if spi_configs := fv.full_config.get().get(CONF_SPI):
        variant = get_esp32_variant()
        if variant in (VARIANT_ESP32C3, VARIANT_ESP32S2, VARIANT_ESP32S3):
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


FINAL_VALIDATE_SCHEMA = _final_validate


def manual_ip(config):
    return cg.StructInitializer(
        ManualIP,
        ("static_ip", IPAddress(*config[CONF_STATIC_IP].args)),
        ("gateway", IPAddress(*config[CONF_GATEWAY].args)),
        ("subnet", IPAddress(*config[CONF_SUBNET].args)),
        ("dns1", IPAddress(*config[CONF_DNS1].args)),
        ("dns2", IPAddress(*config[CONF_DNS2].args)),
    )


def phy_register(address: int, value: int, page: int):
    return cg.StructInitializer(
        PHYRegister,
        ("address", address),
        ("value", value),
        ("page", page),
    )


@coroutine_with_priority(60.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_TYPE] == "W5500":
        cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
        cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
        cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
        cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
        if CONF_INTERRUPT_PIN in config:
            cg.add(var.set_interrupt_pin(config[CONF_INTERRUPT_PIN]))
        if CONF_RESET_PIN in config:
            cg.add(var.set_reset_pin(config[CONF_RESET_PIN]))
        cg.add(var.set_clock_speed(config[CONF_CLOCK_SPEED]))

        cg.add_define("USE_ETHERNET_SPI")
        if CORE.using_esp_idf:
            add_idf_sdkconfig_option("CONFIG_ETH_USE_SPI_ETHERNET", True)
            add_idf_sdkconfig_option("CONFIG_ETH_SPI_ETHERNET_W5500", True)
    elif config[CONF_TYPE] == "OPENETH":
        cg.add_define("USE_ETHERNET_OPENETH")
        add_idf_sdkconfig_option("CONFIG_ETH_USE_OPENETH", True)
    else:
        cg.add(var.set_phy_addr(config[CONF_PHY_ADDR]))
        cg.add(var.set_mdc_pin(config[CONF_MDC_PIN]))
        cg.add(var.set_mdio_pin(config[CONF_MDIO_PIN]))
        cg.add(var.set_clk_mode(*CLK_MODES[config[CONF_CLK_MODE]]))
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
        cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    cg.add_define("USE_ETHERNET")

    if CORE.using_arduino:
        cg.add_library("WiFi", None)
