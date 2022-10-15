from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DOMAIN,
    CONF_ID,
    CONF_MANUAL_IP,
    CONF_STATIC_IP,
    CONF_TYPE,
    CONF_USE_ADDRESS,
    CONF_GATEWAY,
    CONF_SUBNET,
    CONF_DNS1,
    CONF_DNS2,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.components.network import IPAddress

CONFLICTS_WITH = ["wifi"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]

ethernet_ns = cg.esphome_ns.namespace("ethernet")
CONF_PHY_ADDR = "phy_addr"
CONF_MDC_PIN = "mdc_pin"
CONF_MDIO_PIN = "mdio_pin"
CONF_CLK_MODE = "clk_mode"
CONF_POWER_PIN = "power_pin"

EthernetType = ethernet_ns.enum("EthernetType")
ETHERNET_TYPES = {
    "LAN8720": EthernetType.ETHERNET_TYPE_LAN8720,
    "TLK110": EthernetType.ETHERNET_TYPE_TLK110,
    "IP101": EthernetType.ETHERNET_TYPE_IP101,
}

eth_clock_mode_t = cg.global_ns.enum("eth_clock_mode_t")
CLK_MODES = {
    "GPIO0_IN": eth_clock_mode_t.ETH_CLOCK_GPIO0_IN,
    "GPIO0_OUT": eth_clock_mode_t.ETH_CLOCK_GPIO0_OUT,
    "GPIO16_OUT": eth_clock_mode_t.ETH_CLOCK_GPIO16_OUT,
    "GPIO17_OUT": eth_clock_mode_t.ETH_CLOCK_GPIO17_OUT,
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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EthernetComponent),
            cv.Required(CONF_TYPE): cv.enum(ETHERNET_TYPES, upper=True),
            cv.Required(CONF_MDC_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MDIO_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLK_MODE, default="GPIO0_IN"): cv.enum(
                CLK_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_PHY_ADDR, default=0): cv.int_range(min=0, max=31),
            cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            cv.Optional("enable_mdns"): cv.invalid(
                "This option has been removed. Please use the [disabled] option under the "
                "new mdns component instead."
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
    cv.only_with_arduino,
)


def manual_ip(config):
    return cg.StructInitializer(
        ManualIP,
        ("static_ip", IPAddress(*config[CONF_STATIC_IP].args)),
        ("gateway", IPAddress(*config[CONF_GATEWAY].args)),
        ("subnet", IPAddress(*config[CONF_SUBNET].args)),
        ("dns1", IPAddress(*config[CONF_DNS1].args)),
        ("dns2", IPAddress(*config[CONF_DNS2].args)),
    )


@coroutine_with_priority(60.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_phy_addr(config[CONF_PHY_ADDR]))
    cg.add(var.set_mdc_pin(config[CONF_MDC_PIN]))
    cg.add(var.set_mdio_pin(config[CONF_MDIO_PIN]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_clk_mode(CLK_MODES[config[CONF_CLK_MODE]]))
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    if CONF_POWER_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
        cg.add(var.set_power_pin(pin))

    if CONF_MANUAL_IP in config:
        cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    cg.add_define("USE_ETHERNET")

    if CORE.is_esp32:
        cg.add_library("WiFi", None)
