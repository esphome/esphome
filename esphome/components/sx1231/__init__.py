from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi
from esphome.components.const import (
    CONF_BANDWIDTH,
    CONF_BITRATE,
    CONF_CRC_ENABLE,
    CONF_DEVIATION,
    CONF_MODULATION,
    CONF_ON_PACKET,
    CONF_PA_POWER,
    CONF_PA_RAMP,
    CONF_PACKET_MODE,
    CONF_PAYLOAD_LENGTH,
    CONF_PREAMBLE_SIZE,
    CONF_RST_PIN,
    CONF_RX_START,
    CONF_SHAPING,
    CONF_SYNC_VALUE,
)
from esphome.components.packet_interface.radio_link import (
    MARGIN,
    TRANSMIT_TIMEOUT_MS,
    packet_airtime_ms,
)
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_FREQUENCY, CONF_NUMBER
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

DOMAIN = "sx1231"
MULTI_CONF = True
CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["spi"]

CONF_SX1231_ID = "sx1231_id"

CONF_BITSYNC = "bitsync"
CONF_DIO0_PIN = "dio0_pin"
CONF_PA_PIN = "pa_pin"
CONF_RX_FLOOR = "rx_floor"

sx1231_ns = cg.esphome_ns.namespace("sx1231")
SX1231 = sx1231_ns.class_("SX1231", cg.Component, spi.SPIDevice)
SX1231Listener = sx1231_ns.class_("SX1231Listener")
SX1231DataModul = sx1231_ns.enum("SX1231DataModul")
SX1231PaLevel = sx1231_ns.enum("SX1231PaLevel")
SX1231PaRamp = sx1231_ns.enum("SX1231PaRamp")

# RegRxBw codes for FSK: RxBwMant in bits 4:3 and RxBwExp in bits 2:0
BW = {
    "2_6kHz": 0x17,
    "3_1kHz": 0x0F,
    "3_9kHz": 0x07,
    "5_2kHz": 0x16,
    "6_3kHz": 0x0E,
    "7_8kHz": 0x06,
    "10_4kHz": 0x15,
    "12_5kHz": 0x0D,
    "15_6kHz": 0x05,
    "20_8kHz": 0x14,
    "25_0kHz": 0x0C,
    "31_3kHz": 0x04,
    "41_7kHz": 0x13,
    "50_0kHz": 0x0B,
    "62_5kHz": 0x03,
    "83_3kHz": 0x12,
    "100_0kHz": 0x0A,
    "125_0kHz": 0x02,
    "166_7kHz": 0x11,
    "200_0kHz": 0x09,
    "250_0kHz": 0x01,
    "500_0kHz": 0x00,
}

MOD = SX1231DataModul.options("FSK", "OOK", prefix="MOD_")

FXOSC = 32_000_000
# The synthesizer does not cover the gaps between these bands
FREQUENCY_BANDS = ((290, 340), (424, 510), (862, 1020))
MIN_FSK_DEVIATION = 600
MAX_OOK_BITRATE = 32768

# RFO is the output of the RFM69W and RFM69CW, BOOST that of the RFM69HW and RFM69HCW
PA_PIN = SX1231PaLevel.options("RFO", "BOOST", prefix="PA_PIN_")

RAMP = {
    "10us": SX1231PaRamp.PA_RAMP_10,
    "12us": SX1231PaRamp.PA_RAMP_12,
    "15us": SX1231PaRamp.PA_RAMP_15,
    "20us": SX1231PaRamp.PA_RAMP_20,
    "25us": SX1231PaRamp.PA_RAMP_25,
    "31us": SX1231PaRamp.PA_RAMP_31,
    "40us": SX1231PaRamp.PA_RAMP_40,
    "50us": SX1231PaRamp.PA_RAMP_50,
    "62us": SX1231PaRamp.PA_RAMP_62,
    "100us": SX1231PaRamp.PA_RAMP_100,
    "125us": SX1231PaRamp.PA_RAMP_125,
    "250us": SX1231PaRamp.PA_RAMP_250,
    "500us": SX1231PaRamp.PA_RAMP_500,
    "1000us": SX1231PaRamp.PA_RAMP_1000,
    "2000us": SX1231PaRamp.PA_RAMP_2000,
    "3400us": SX1231PaRamp.PA_RAMP_3400,
}

SHAPING = {
    "CUTOFF_BR_X_2": SX1231DataModul.CUTOFF_BR_X_2,
    "CUTOFF_BR_X_1": SX1231DataModul.CUTOFF_BR_X_1,
    "GAUSSIAN_BT_0_3": SX1231DataModul.GAUSSIAN_BT_0_3,
    "GAUSSIAN_BT_0_5": SX1231DataModul.GAUSSIAN_BT_0_5,
    "GAUSSIAN_BT_1_0": SX1231DataModul.GAUSSIAN_BT_1_0,
    "NONE": SX1231DataModul.SHAPING_NONE,
}


@schema_extractor("schema")
def validate_raw_data(value: Any) -> bytes | list[int]:
    if value is SCHEMA_EXTRACT:
        return cv.ensure_list(cv.hex_uint8_t)
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


# The radio's 66 byte FIFO also holds the length byte, and empty packets are rejected.
MAX_PACKET_SIZE = 64
validate_packet_data = cv.All(validate_raw_data, cv.Length(min=1, max=MAX_PACKET_SIZE))


def _bandwidth_hz(name: str) -> float:
    code = BW[name]
    return FXOSC / ((16 + 4 * (code >> 3)) << ((code & 0x07) + 2))


def validate_config(config: ConfigType) -> ConfigType:
    if not any(
        low * 1e6 <= config[CONF_FREQUENCY] <= high * 1e6
        for low, high in FREQUENCY_BANDS
    ):
        bands = ", ".join(f"{low} to {high} MHz" for low, high in FREQUENCY_BANDS)
        raise cv.Invalid(f"The radio only covers {bands}", path=[CONF_FREQUENCY])
    if config[CONF_MODULATION] == "OOK":
        if config[CONF_BANDWIDTH] == "500_0kHz":
            raise cv.Invalid(
                "500_0kHz is not available with OOK", path=[CONF_BANDWIDTH]
            )
        if config[CONF_SHAPING].startswith("GAUSSIAN"):
            raise cv.Invalid(
                f"{config[CONF_SHAPING]} is only available with FSK",
                path=[CONF_SHAPING],
            )
        if config[CONF_BITRATE] > MAX_OOK_BITRATE:
            raise cv.Invalid(
                f"The bitrate cannot be above {MAX_OOK_BITRATE} with OOK",
                path=[CONF_BITRATE],
            )
    else:
        if config[CONF_SHAPING].startswith("CUTOFF"):
            raise cv.Invalid(
                f"{config[CONF_SHAPING]} is only available with OOK",
                path=[CONF_SHAPING],
            )
        if config[CONF_DEVIATION] < MIN_FSK_DEVIATION:
            raise cv.Invalid(
                f"The deviation must be at least {MIN_FSK_DEVIATION} Hz with FSK",
                path=[CONF_DEVIATION],
            )
    # The receiver cannot sample a bitrate of twice its bandwidth or more. The radio divides its clock by a whole
    # number, so its bitrate can be above the configured one.
    bitrate = FXOSC / ((FXOSC + config[CONF_BITRATE] // 2) // config[CONF_BITRATE])
    if bitrate >= 2 * (width := _bandwidth_hz(config[CONF_BANDWIDTH])):
        raise cv.Invalid(
            f"The radio's nearest bitrate is {bitrate:.0f}, which must be below {2 * width:.0f}, twice the "
            f"'{CONF_BANDWIDTH}'",
            path=[CONF_BITRATE],
        )
    if config[CONF_PACKET_MODE]:
        if CONF_DIO0_PIN not in config:
            raise cv.Invalid("Config 'dio0_pin' required in packet mode")
        # DIO0's interrupt wakes the loop when a packet has arrived
        if CORE.is_esp8266 and config[CONF_DIO0_PIN][CONF_NUMBER] == 16:
            raise cv.Invalid(
                "GPIO16 has no interrupt, which packet mode needs on this pin",
                path=[CONF_DIO0_PIN],
            )
        if not config[CONF_BITSYNC]:
            raise cv.Invalid(
                "Config 'bitsync' cannot be turned off in packet mode",
                path=[CONF_BITSYNC],
            )
        # The driver waits TRANSMIT_TIMEOUT_MS for a packet to be sent; the radio rounds the bitrate, hence the margin
        size = config[CONF_PAYLOAD_LENGTH] or MAX_PACKET_SIZE
        limit = TRANSMIT_TIMEOUT_MS / MARGIN
        if (airtime := packet_airtime_ms(config, size)) > limit:
            raise cv.Invalid(
                f"With this '{CONF_BITRATE}' and '{CONF_PREAMBLE_SIZE}' a packet of {size} bytes takes "
                f"{airtime:.0f} ms to send, and the limit is {limit:.0f} ms",
                path=[CONF_PREAMBLE_SIZE],
            )
    if 0x00 in config[CONF_SYNC_VALUE]:
        raise cv.Invalid(
            "The radio does not support a sync value byte of 0x00",
            path=[CONF_SYNC_VALUE],
        )
    if config[CONF_PA_PIN] == "RFO" and config[CONF_PA_POWER] > 13:
        raise cv.Invalid("PA power must be <= 13 dbm when using the RFO pin")
    if config[CONF_PA_PIN] == "BOOST" and config[CONF_PA_POWER] < -2:
        raise cv.Invalid("PA power must be >= -2 dbm when using the BOOST pin")
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SX1231),
            cv.Optional(CONF_BANDWIDTH, default="125_0kHz"): cv.enum(BW),
            cv.Optional(CONF_BITRATE, default=4800): cv.int_range(min=500, max=300000),
            cv.Optional(CONF_BITSYNC, default=True): cv.boolean,
            cv.Optional(CONF_CRC_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_DEVIATION, default="5kHz"): cv.All(
                cv.frequency, cv.int_range(min=0, max=300000)
            ),
            cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_FREQUENCY): cv.All(
                cv.frequency, cv.int_range(min=int(290e6), max=int(1020e6))
            ),
            cv.Required(CONF_MODULATION): cv.enum(MOD),
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(single=True),
            cv.Optional(CONF_PA_PIN, default="BOOST"): cv.enum(PA_PIN),
            cv.Optional(CONF_PA_POWER, default=13): cv.int_range(min=-18, max=20),
            cv.Optional(CONF_PA_RAMP, default="40us"): cv.enum(RAMP),
            cv.Optional(CONF_PACKET_MODE, default=True): cv.boolean,
            cv.Optional(CONF_PAYLOAD_LENGTH, default=0): cv.int_range(
                min=0, max=MAX_PACKET_SIZE
            ),
            cv.Optional(CONF_PREAMBLE_SIZE, default=3): cv.uint16_t,
            cv.Optional(CONF_RST_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RX_FLOOR, default=-94): cv.float_range(min=-127, max=-1),
            cv.Optional(CONF_RX_START, default=True): cv.boolean,
            cv.Optional(CONF_SHAPING, default="NONE"): cv.enum(SHAPING),
            cv.Optional(CONF_SYNC_VALUE, default=[]): cv.All(
                cv.ensure_list(cv.hex_uint8_t), cv.Length(max=8)
            ),
        },
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(True, 8e6, "mode0"))
    .add_extra(validate_config)
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    DOMAIN, require_miso=True, require_mosi=True
)


async def to_code(config: ConfigType) -> None:
    var = await spi.new_spi_component(config)
    if CONF_ON_PACKET in config:
        await automation.build_automation(
            var.get_packet_trigger(),
            [
                (cg.std_vector.template(cg.uint8), "x"),
                (cg.float_, "rssi"),
                (cg.float_, "snr"),
            ],
            config[CONF_ON_PACKET],
        )
    await cg.add_pin(var.set_dio0_pin, config.get(CONF_DIO0_PIN))
    await cg.add_pin(var.set_rst_pin, config.get(CONF_RST_PIN))
    cg.add(var.set_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_deviation(config[CONF_DEVIATION]))
    cg.add(var.set_modulation(config[CONF_MODULATION]))
    cg.add(var.set_bitrate(config[CONF_BITRATE]))
    cg.add(var.set_bitsync(config[CONF_BITSYNC]))
    cg.add(var.set_packet_mode(config[CONF_PACKET_MODE]))
    cg.add(var.set_pa_pin(config[CONF_PA_PIN]))
    cg.add(var.set_pa_ramp(config[CONF_PA_RAMP]))
    cg.add(var.set_pa_power(config[CONF_PA_POWER]))
    cg.add(var.set_shaping(config[CONF_SHAPING]))
    cg.add(var.set_crc_enable(config[CONF_CRC_ENABLE]))
    cg.add(var.set_payload_length(config[CONF_PAYLOAD_LENGTH]))
    cg.add(var.set_preamble_size(config[CONF_PREAMBLE_SIZE]))
    cg.add(var.set_sync_value(config[CONF_SYNC_VALUE]))
    cg.add(var.set_rx_floor(config[CONF_RX_FLOOR]))
    cg.add(var.set_rx_start(config[CONF_RX_START]))


NO_ARGS_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SX1231),
    }
)


for _name, _call in (
    ("sx1231.set_mode_tx", "set_mode_tx()"),
    ("sx1231.set_mode_rx", "set_mode_rx()"),
    ("sx1231.set_mode_sleep", "set_mode_sleep()"),
    ("sx1231.set_mode_standby", "set_mode_standby()"),
):
    automation.register_apply_action(
        _name, NO_ARGS_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


SEND_PACKET_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(SX1231),
        cv.Required(CONF_DATA): cv.templatable(validate_packet_data),
    },
    key=CONF_DATA,
)


automation.register_apply_action(
    "sx1231.send_packet",
    SEND_PACKET_ACTION_SCHEMA,
    automation.ApplyBytes(CONF_DATA, "transmit_packet({})", stack=MAX_PACKET_SIZE),
)
