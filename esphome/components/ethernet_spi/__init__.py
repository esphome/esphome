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

ethernet_ns = cg.esphome_ns.namespace("ethernet_spi")
CONF_PHY_ADDR = "phy_addr"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_SCLK_PIN = "sclk_pin"
CONF_POWER_PIN = "power_pin"
CONF_SCS_PIN="cs_pin"
CONF_CLK_SPEED="clk_speed"

EthernetType = ethernet_ns.enum("EthernetType_spi")
ETHERNET_TYPES = {
    "W5500": EthernetType.ETHERNET_TYPE_W5500,
    "DM9051": EthernetType.ETHERNET_TYPE_DM9051,
    "KSZ8851SNL": EthernetType.ETHERNET_TYPE_KSZ8851SNL,
}


#####it seems that rmii is useless when choose the spi



# emac_rmii_clock_mode_t = cg.global_ns.enum("emac_rmii_clock_mode_t")
# emac_rmii_clock_gpio_t = cg.global_ns.enum("emac_rmii_clock_gpio_t")
# CLK_MODES = {
#     "GPIO0_IN": (
#         emac_rmii_clock_mode_t.EMAC_CLK_EXT_IN,
#         emac_rmii_clock_gpio_t.EMAC_CLK_IN_GPIO,
#     ),
#     "GPIO0_OUT": (
#         emac_rmii_clock_mode_t.EMAC_CLK_OUT,
#         emac_rmii_clock_gpio_t.EMAC_APPL_CLK_OUT_GPIO,
#     ),
#     "GPIO16_OUT": (
#         emac_rmii_clock_mode_t.EMAC_CLK_OUT,
#         emac_rmii_clock_gpio_t.EMAC_CLK_OUT_GPIO,
#     ),
#     "GPIO17_OUT": (
#         emac_rmii_clock_mode_t.EMAC_CLK_OUT,
#         emac_rmii_clock_gpio_t.EMAC_CLK_OUT_180_GPIO,
#     ),
# }


MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4,
        cv.Required(CONF_GATEWAY): cv.ipv4,
        cv.Required(CONF_SUBNET): cv.ipv4,
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4,
    }
)

EthernetComponent = ethernet_ns.class_("Ethernet_SPI_component", cg.Component)
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
            cv.Required(CONF_MISO_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_SCLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_SCS_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLK_SPEED,default=5):cv.int_range(min=1,max=50),
            cv.Optional(CONF_PHY_ADDR, default=0): cv.int_range(min=0, max=31),
            cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_number,
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
    cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
    cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
    cg.add(var.set_cs_pin(config[CONF_SCS_PIN]))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_clockspeed(config[CONF_CLK_SPEED]))
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    if CONF_POWER_PIN in config:
        cg.add(var.set_power_pin(config[CONF_POWER_PIN]))

    if CONF_MANUAL_IP in config:
        cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    cg.add_define("USE_ETHERNET")

    if CORE.using_arduino:
        cg.add_library("WiFi", None)
