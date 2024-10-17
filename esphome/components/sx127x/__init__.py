from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["spi"]

CONF_PA_POWER = "pa_power"
CONF_PA_PIN = "pa_pin"
CONF_NSS_PIN = "nss_pin"
CONF_RST_PIN = "rst_pin"
CONF_MODULATION = "modulation"
CONF_RX_FLOOR = "rx_floor"
CONF_RX_START = "rx_start"
CONF_RX_BANDWIDTH = "rx_bandwidth"
CONF_FSK_FDEV = "fsk_fdev"
CONF_FSK_RAMP = "fsk_ramp"
CONF_FSK_SHAPING = "fsk_shaping"

sx127x_ns = cg.esphome_ns.namespace("sx127x")
SX127x = sx127x_ns.class_("SX127x", cg.Component, spi.SPIDevice)
SX127xOpMode = sx127x_ns.enum("SX127xOpMode")
SX127xRxBw = sx127x_ns.enum("SX127xRxBw")
SX127xPaConfig = sx127x_ns.enum("SX127xPaConfig")
SX127xPaRamp = sx127x_ns.enum("SX127xPaRamp")

PA_PIN = {
    "RFO": SX127xPaConfig.PA_PIN_RFO,
    "BOOST": SX127xPaConfig.PA_PIN_BOOST,
}

SHAPING = {
    "BT_0_3": SX127xPaRamp.SHAPING_BT_0_3,
    "BT_0_5": SX127xPaRamp.SHAPING_BT_0_5,
    "BT_1_0": SX127xPaRamp.SHAPING_BT_1_0,
    "NONE": SX127xPaRamp.SHAPING_NONE,
}

RAMP = {
    "10us": SX127xPaRamp.PA_RAMP_10,
    "12us": SX127xPaRamp.PA_RAMP_12,
    "15us": SX127xPaRamp.PA_RAMP_15,
    "20us": SX127xPaRamp.PA_RAMP_20,
    "25us": SX127xPaRamp.PA_RAMP_25,
    "31us": SX127xPaRamp.PA_RAMP_31,
    "40us": SX127xPaRamp.PA_RAMP_40,
    "50us": SX127xPaRamp.PA_RAMP_50,
    "62us": SX127xPaRamp.PA_RAMP_62,
    "100us": SX127xPaRamp.PA_RAMP_100,
    "125us": SX127xPaRamp.PA_RAMP_125,
    "250us": SX127xPaRamp.PA_RAMP_250,
    "500us": SX127xPaRamp.PA_RAMP_500,
    "1000us": SX127xPaRamp.PA_RAMP_1000,
    "2000us": SX127xPaRamp.PA_RAMP_2000,
    "3400us": SX127xPaRamp.PA_RAMP_3400,
}

MOD = {
    "FSK": SX127xOpMode.MOD_FSK,
    "OOK": SX127xOpMode.MOD_OOK,
}

RX_BW = {
    "2_6kHz": SX127xRxBw.RX_BW_2_6,
    "3_1kHz": SX127xRxBw.RX_BW_3_1,
    "3_9kHz": SX127xRxBw.RX_BW_3_9,
    "5_2kHz": SX127xRxBw.RX_BW_5_2,
    "6_3kHz": SX127xRxBw.RX_BW_6_3,
    "7_8kHz": SX127xRxBw.RX_BW_7_8,
    "10_4kHz": SX127xRxBw.RX_BW_10_4,
    "12_5kHz": SX127xRxBw.RX_BW_12_5,
    "15_6kHz": SX127xRxBw.RX_BW_15_6,
    "20_8kHz": SX127xRxBw.RX_BW_20_8,
    "25_0kHz": SX127xRxBw.RX_BW_25_0,
    "31_3kHz": SX127xRxBw.RX_BW_31_3,
    "41_7kHz": SX127xRxBw.RX_BW_41_7,
    "50_0kHz": SX127xRxBw.RX_BW_50_0,
    "62_5kHz": SX127xRxBw.RX_BW_62_5,
    "83_3kHz": SX127xRxBw.RX_BW_83_3,
    "100_0kHz": SX127xRxBw.RX_BW_100_0,
    "125_0kHz": SX127xRxBw.RX_BW_125_0,
    "166_7kHz": SX127xRxBw.RX_BW_166_7,
    "200_0kHz": SX127xRxBw.RX_BW_200_0,
    "250_0kHz": SX127xRxBw.RX_BW_250_0,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SX127x),
        cv.Required(CONF_RST_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_NSS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_FREQUENCY): cv.int_range(min=137000000, max=1020000000),
        cv.Required(CONF_MODULATION): cv.enum(MOD),
        cv.Optional(CONF_FSK_FDEV, default=5000): cv.int_range(min=0, max=100000),
        cv.Optional(CONF_FSK_RAMP, default="40us"): cv.enum(RAMP),
        cv.Optional(CONF_FSK_SHAPING, default="NONE"): cv.enum(SHAPING),
        cv.Optional(CONF_RX_FLOOR, default=-94): cv.float_range(min=-128, max=-1),
        cv.Optional(CONF_RX_START, default=True): cv.boolean,
        cv.Optional(CONF_RX_BANDWIDTH, default="50_0kHz"): cv.enum(RX_BW),
        cv.Optional(CONF_PA_PIN, default="BOOST"): cv.enum(PA_PIN),
        cv.Optional(CONF_PA_POWER, default=17): cv.int_range(min=0, max=17),
    }
).extend(spi.spi_device_schema(False, 8e6, "mode0"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    rst_pin = await cg.gpio_pin_expression(config[CONF_RST_PIN])
    cg.add(var.set_rst_pin(rst_pin))
    nss_pin = await cg.gpio_pin_expression(config[CONF_NSS_PIN])
    cg.add(var.set_nss_pin(nss_pin))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_modulation(config[CONF_MODULATION]))
    cg.add(var.set_rx_floor(config[CONF_RX_FLOOR]))
    cg.add(var.set_rx_start(config[CONF_RX_START]))
    cg.add(var.set_rx_bandwidth(config[CONF_RX_BANDWIDTH]))
    cg.add(var.set_pa_pin(config[CONF_PA_PIN]))
    cg.add(var.set_pa_power(config[CONF_PA_POWER]))
    cg.add(var.set_fsk_fdev(config[CONF_FSK_FDEV]))
    cg.add(var.set_fsk_ramp(config[CONF_FSK_RAMP]))
    cg.add(var.set_fsk_shaping(config[CONF_FSK_SHAPING]))
