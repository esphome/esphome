from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID
from esphome.core import TimePeriod

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["spi"]

CONF_PA_POWER = "pa_power"
CONF_PA_PIN = "pa_pin"
CONF_DIO0_PIN = "dio0_pin"
CONF_DIO2_PIN = "dio2_pin"
CONF_NSS_PIN = "nss_pin"
CONF_RST_PIN = "rst_pin"
CONF_MODULATION = "modulation"
CONF_SHAPING = "shaping"
CONF_RX_FLOOR = "rx_floor"
CONF_RX_START = "rx_start"
CONF_RX_BANDWIDTH = "rx_bandwidth"
CONF_RX_DURATION = "rx_duration"
CONF_BITRATE = "bitrate"
CONF_PAYLOAD_LENGTH = "payload_length"
CONF_PREAMBLE_SIZE = "preamble_size"
CONF_PREAMBLE_POLARITY = "preamble_polarity"
CONF_PREAMBLE_ERRORS = "preamble_errors"
CONF_SYNC_VALUE = "sync_value"
CONF_FSK_FDEV = "fsk_fdev"
CONF_FSK_RAMP = "fsk_ramp"
CONF_ON_PACKET = "on_packet"

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
    "CUTOFF_BR_X_2": SX127xPaRamp.CUTOFF_BR_X_2,
    "CUTOFF_BR_X_1": SX127xPaRamp.CUTOFF_BR_X_1,
    "GAUSSIAN_BT_0_3": SX127xPaRamp.GAUSSIAN_BT_0_3,
    "GAUSSIAN_BT_0_5": SX127xPaRamp.GAUSSIAN_BT_0_5,
    "GAUSSIAN_BT_1_0": SX127xPaRamp.GAUSSIAN_BT_1_0,
    "NO_SHAPING": SX127xPaRamp.NO_SHAPING,
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
        cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_DIO2_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_RST_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_NSS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_FREQUENCY): cv.int_range(min=137000000, max=1020000000),
        cv.Required(CONF_MODULATION): cv.enum(MOD),
        cv.Optional(CONF_SHAPING, default="NO_SHAPING"): cv.enum(SHAPING),
        cv.Optional(CONF_BITRATE, default=0): cv.int_range(min=0, max=300000),
        cv.Optional(CONF_FSK_FDEV, default=5000): cv.int_range(min=0, max=100000),
        cv.Optional(CONF_FSK_RAMP, default="40us"): cv.enum(RAMP),
        cv.Optional(CONF_SYNC_VALUE, default=[]): cv.ensure_list(cv.hex_uint8_t),
        cv.Optional(CONF_PAYLOAD_LENGTH, default=0): cv.int_range(min=0, max=64),
        cv.Optional(CONF_PREAMBLE_SIZE, default=0): cv.int_range(min=0, max=7),
        cv.Optional(CONF_PREAMBLE_POLARITY, default=0xAA): cv.All(
            cv.hex_int, cv.one_of(0xAA, 0x55)
        ),
        cv.Optional(CONF_PREAMBLE_ERRORS, default=0): cv.int_range(min=0, max=31),
        cv.Optional(CONF_RX_FLOOR, default=-94): cv.float_range(min=-128, max=-1),
        cv.Optional(CONF_RX_START, default=True): cv.boolean,
        cv.Optional(CONF_RX_BANDWIDTH, default="50_0kHz"): cv.enum(RX_BW),
        cv.Optional(CONF_RX_DURATION, default="0ms"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(max=TimePeriod(microseconds=1000000000)),
        ),
        cv.Optional(CONF_PA_PIN, default="BOOST"): cv.enum(PA_PIN),
        cv.Optional(CONF_PA_POWER, default=17): cv.int_range(min=0, max=17),
        cv.Optional(CONF_ON_PACKET): automation.validate_automation(single=True),
    }
).extend(spi.spi_device_schema(False, 8e6, "mode0"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    if CONF_ON_PACKET in config:
        await automation.build_automation(
            var.get_packet_trigger(),
            [(cg.std_vector.template(cg.uint8), "x")],
            config[CONF_ON_PACKET],
        )
    if CONF_DIO0_PIN in config:
        dio0_pin = await cg.gpio_pin_expression(config[CONF_DIO0_PIN])
        cg.add(var.set_dio0_pin(dio0_pin))
    if CONF_DIO2_PIN in config:
        dio2_pin = await cg.gpio_pin_expression(config[CONF_DIO2_PIN])
        cg.add(var.set_dio2_pin(dio2_pin))
    rst_pin = await cg.gpio_pin_expression(config[CONF_RST_PIN])
    cg.add(var.set_rst_pin(rst_pin))
    nss_pin = await cg.gpio_pin_expression(config[CONF_NSS_PIN])
    cg.add(var.set_nss_pin(nss_pin))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_modulation(config[CONF_MODULATION]))
    cg.add(var.set_shaping(config[CONF_SHAPING]))
    cg.add(var.set_bitrate(config[CONF_BITRATE]))
    cg.add(var.set_payload_length(config[CONF_PAYLOAD_LENGTH]))
    cg.add(var.set_preamble_size(config[CONF_PREAMBLE_SIZE]))
    cg.add(var.set_preamble_polarity(config[CONF_PREAMBLE_POLARITY]))
    cg.add(var.set_preamble_errors(config[CONF_PREAMBLE_ERRORS]))
    cg.add(var.set_sync_value(config[CONF_SYNC_VALUE]))
    cg.add(var.set_rx_floor(config[CONF_RX_FLOOR]))
    cg.add(var.set_rx_start(config[CONF_RX_START]))
    cg.add(var.set_rx_bandwidth(config[CONF_RX_BANDWIDTH]))
    cg.add(var.set_rx_duration(config[CONF_RX_DURATION]))
    cg.add(var.set_pa_pin(config[CONF_PA_PIN]))
    cg.add(var.set_pa_power(config[CONF_PA_POWER]))
    cg.add(var.set_fsk_fdev(config[CONF_FSK_FDEV]))
    cg.add(var.set_fsk_ramp(config[CONF_FSK_RAMP]))
