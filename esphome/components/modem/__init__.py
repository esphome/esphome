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
from esphome.components.esp32 import add_idf_component
from esphome.components.network import IPAddress

CONFLICTS_WITH = ["wifi"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]

modem_ns = cg.esphome_ns.namespace("modem")
# CONF_PHY_ADDR = "phy_addr"
# CONF_MDC_PIN = "mdc_pin"
# CONF_MDIO_PIN = "mdio_pin"
# CONF_CLK_MODE = "clk_mode"
# CONF_POWER_PIN = "power_pin"

ModemType = modem_ns.enum("ModemType")
MODEM_TYPES = {
    "BG96": ModemType.MODEM_TYPE_BG96,
    "SIM800": ModemType.MODEM_TYPE_SIM800,
    "SIM7000": ModemType.MODEM_TYPE_SIM7000,
    "SIM7070": ModemType.MODEM_TYPE_SIM7070,
}

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


# MANUAL_IP_SCHEMA = cv.Schema(
#     {
#         cv.Required(CONF_STATIC_IP): cv.ipv4,
#         cv.Required(CONF_GATEWAY): cv.ipv4,
#         cv.Required(CONF_SUBNET): cv.ipv4,
#         cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4,
#         cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4,
#     }
# )

ModemComponent = modem_ns.class_("ModemComponent", cg.Component)
#ManualIP = ethernet_ns.struct("ManualIP")


def _validate(config):
    if CONF_USE_ADDRESS not in config:
        use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemComponent),
            cv.Required(CONF_TYPE): cv.enum(MODEM_TYPES, upper=True),
            #cv.Required(CONF_MDC_PIN): pins.internal_gpio_output_pin_number,
            # cv.Required(CONF_MDIO_PIN): pins.internal_gpio_output_pin_number,
            # cv.Optional(CONF_CLK_MODE, default="GPIO0_IN"): cv.enum(
            #     CLK_MODES, upper=True, space="_"
            # ),
            # cv.Optional(CONF_PHY_ADDR, default=0): cv.int_range(min=0, max=31),
            # cv.Optional(CONF_POWER_PIN): pins.internal_gpio_output_pin_number,
            # cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
            cv.Optional(CONF_DOMAIN, default=".local"): cv.domain_name,
            cv.Optional(CONF_USE_ADDRESS): cv.string_strict,
            # cv.Optional("enable_mdns"): cv.invalid(
            #     "This option has been removed. Please use the [disabled] option under the "
            #     "new mdns component instead."
            # ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


# def manual_ip(config):
#     return cg.StructInitializer(
#         ManualIP,
#         ("static_ip", IPAddress(*config[CONF_STATIC_IP].args)),
#         ("gateway", IPAddress(*config[CONF_GATEWAY].args)),
#         ("subnet", IPAddress(*config[CONF_SUBNET].args)),
#         ("dns1", IPAddress(*config[CONF_DNS1].args)),
#         ("dns2", IPAddress(*config[CONF_DNS2].args)),
#     )


@coroutine_with_priority(60.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    #cg.add(var.set_phy_addr(config[CONF_PHY_ADDR]))
    #cg.add(var.set_mdc_pin(config[CONF_MDC_PIN]))
    #cg.add(var.set_mdio_pin(config[CONF_MDIO_PIN]))
    cg.add(var.set_type(config[CONF_TYPE]))
    #cg.add(var.set_clk_mode(*CLK_MODES[config[CONF_CLK_MODE]]))
    cg.add(var.set_use_address(config[CONF_USE_ADDRESS]))

    # if CONF_POWER_PIN in config:
    #     cg.add(var.set_power_pin(config[CONF_POWER_PIN]))

    # if CONF_MANUAL_IP in config:
    #     cg.add(var.set_manual_ip(manual_ip(config[CONF_MANUAL_IP])))

    cg.add_define("USE_MODEM")

    if CORE.using_arduino:
        cg.add_library("WiFi", None)
        
    if CORE.using_esp_idf:
        add_idf_component(
            name="esp_modem",
            repo="https://github.com/espressif/esp-protocols.git",
            ref="^1.1.0",
            path="components/esp_modem",
        )
