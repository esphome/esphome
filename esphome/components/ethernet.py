import voluptuous as vol

from esphome import pins
from esphome.components import wifi
import esphome.config_validation as cv
from esphome.const import CONF_DOMAIN, CONF_ID, CONF_MANUAL_IP, CONF_STATIC_IP, CONF_TYPE, \
    CONF_USE_ADDRESS, ESP_PLATFORM_ESP32
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_output_pin_expression
from esphome.cpp_types import App, Component, esphome_ns, global_ns

CONFLICTS_WITH = ['wifi']
ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_PHY_ADDR = 'phy_addr'
CONF_MDC_PIN = 'mdc_pin'
CONF_MDIO_PIN = 'mdio_pin'
CONF_CLK_MODE = 'clk_mode'
CONF_POWER_PIN = 'power_pin'

EthernetType = esphome_ns.enum('EthernetType')
ETHERNET_TYPES = {
    'LAN8720': EthernetType.ETHERNET_TYPE_LAN8720,
    'TLK110': EthernetType.ETHERNET_TYPE_TLK110,
}

eth_clock_mode_t = global_ns.enum('eth_clock_mode_t')
CLK_MODES = {
    'GPIO0_IN': eth_clock_mode_t.ETH_CLOCK_GPIO0_IN,
    'GPIO0_OUT': eth_clock_mode_t.ETH_CLOCK_GPIO0_OUT,
    'GPIO16_OUT': eth_clock_mode_t.ETH_CLOCK_GPIO16_OUT,
    'GPIO17_OUT': eth_clock_mode_t.ETH_CLOCK_GPIO17_OUT,
}

EthernetComponent = esphome_ns.class_('EthernetComponent', Component)


def validate(config):
    if CONF_USE_ADDRESS not in config:
        if CONF_MANUAL_IP in config:
            use_address = str(config[CONF_MANUAL_IP][CONF_STATIC_IP])
        else:
            use_address = CORE.name + config[CONF_DOMAIN]
        config[CONF_USE_ADDRESS] = use_address
    return config


CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(EthernetComponent),
    vol.Required(CONF_TYPE): cv.one_of(*ETHERNET_TYPES, upper=True),
    vol.Required(CONF_MDC_PIN): pins.output_pin,
    vol.Required(CONF_MDIO_PIN): pins.input_output_pin,
    vol.Optional(CONF_CLK_MODE, default='GPIO0_IN'): cv.one_of(*CLK_MODES, upper=True, space='_'),
    vol.Optional(CONF_PHY_ADDR, default=0): vol.All(cv.int_, vol.Range(min=0, max=31)),
    vol.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_MANUAL_IP): wifi.STA_MANUAL_IP_SCHEMA,
    vol.Optional(CONF_DOMAIN, default='.local'): cv.domain_name,
    vol.Optional(CONF_USE_ADDRESS): cv.string_strict,

    vol.Optional('hostname'): cv.invalid("The hostname option has been removed in 1.11.0"),
}), validate)


def to_code(config):
    rhs = App.init_ethernet()
    eth = Pvariable(config[CONF_ID], rhs)

    add(eth.set_phy_addr(config[CONF_PHY_ADDR]))
    add(eth.set_mdc_pin(config[CONF_MDC_PIN]))
    add(eth.set_mdio_pin(config[CONF_MDIO_PIN]))
    add(eth.set_type(ETHERNET_TYPES[config[CONF_TYPE]]))
    add(eth.set_clk_mode(CLK_MODES[config[CONF_CLK_MODE]]))
    add(eth.set_use_address(config[CONF_USE_ADDRESS]))

    if CONF_POWER_PIN in config:
        for pin in gpio_output_pin_expression(config[CONF_POWER_PIN]):
            yield
        add(eth.set_power_pin(pin))

    if CONF_MANUAL_IP in config:
        add(eth.set_manual_ip(wifi.manual_ip(config[CONF_MANUAL_IP])))


REQUIRED_BUILD_FLAGS = '-DUSE_ETHERNET'
