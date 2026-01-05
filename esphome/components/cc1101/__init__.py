from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import spi
from esphome.components.const import CONF_CRC_ENABLE, CONF_ON_PACKET
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_DATA,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_WAIT_TIME,
)
from esphome.core import ID

CODEOWNERS = ["@lygris", "@gabest11"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

ns = cg.esphome_ns.namespace("cc1101")
CC1101Component = ns.class_("CC1101Component", cg.Component, spi.SPIDevice)

# Config keys
CONF_OUTPUT_POWER = "output_power"
CONF_RX_ATTENUATION = "rx_attenuation"
CONF_DC_BLOCKING_FILTER = "dc_blocking_filter"
CONF_IF_FREQUENCY = "if_frequency"
CONF_FILTER_BANDWIDTH = "filter_bandwidth"
CONF_CHANNEL_SPACING = "channel_spacing"
CONF_FSK_DEVIATION = "fsk_deviation"
CONF_MSK_DEVIATION = "msk_deviation"
CONF_SYMBOL_RATE = "symbol_rate"
CONF_SYNC_MODE = "sync_mode"
CONF_CARRIER_SENSE_ABOVE_THRESHOLD = "carrier_sense_above_threshold"
CONF_MODULATION_TYPE = "modulation_type"
CONF_MANCHESTER = "manchester"
CONF_NUM_PREAMBLE = "num_preamble"
CONF_SYNC1 = "sync1"
CONF_SYNC0 = "sync0"
CONF_MAGN_TARGET = "magn_target"
CONF_MAX_LNA_GAIN = "max_lna_gain"
CONF_MAX_DVGA_GAIN = "max_dvga_gain"
CONF_CARRIER_SENSE_ABS_THR = "carrier_sense_abs_thr"
CONF_CARRIER_SENSE_REL_THR = "carrier_sense_rel_thr"
CONF_LNA_PRIORITY = "lna_priority"
CONF_FILTER_LENGTH_FSK_MSK = "filter_length_fsk_msk"
CONF_FILTER_LENGTH_ASK_OOK = "filter_length_ask_ook"
CONF_FREEZE = "freeze"
CONF_HYST_LEVEL = "hyst_level"

# Packet mode config keys
CONF_PACKET_MODE = "packet_mode"
CONF_PACKET_LENGTH = "packet_length"
CONF_WHITENING = "whitening"
CONF_GDO0_PIN = "gdo0_pin"

# Enums
SyncMode = ns.enum("SyncMode", True)
SYNC_MODE = {
    "None": SyncMode.SYNC_MODE_NONE,
    "15/16": SyncMode.SYNC_MODE_15_16,
    "16/16": SyncMode.SYNC_MODE_16_16,
    "30/32": SyncMode.SYNC_MODE_30_32,
}

Modulation = ns.enum("Modulation", True)
MODULATION = {
    "2-FSK": Modulation.MODULATION_2_FSK,
    "GFSK": Modulation.MODULATION_GFSK,
    "ASK/OOK": Modulation.MODULATION_ASK_OOK,
    "4-FSK": Modulation.MODULATION_4_FSK,
    "MSK": Modulation.MODULATION_MSK,
}

RxAttenuation = ns.enum("RxAttenuation", True)
RX_ATTENUATION = {
    "0dB": RxAttenuation.RX_ATTENUATION_0DB,
    "6dB": RxAttenuation.RX_ATTENUATION_6DB,
    "12dB": RxAttenuation.RX_ATTENUATION_12DB,
    "18dB": RxAttenuation.RX_ATTENUATION_18DB,
}

MagnTarget = ns.enum("MagnTarget", True)
MAGN_TARGET = {
    "24dB": MagnTarget.MAGN_TARGET_24DB,
    "27dB": MagnTarget.MAGN_TARGET_27DB,
    "30dB": MagnTarget.MAGN_TARGET_30DB,
    "33dB": MagnTarget.MAGN_TARGET_33DB,
    "36dB": MagnTarget.MAGN_TARGET_36DB,
    "38dB": MagnTarget.MAGN_TARGET_38DB,
    "40dB": MagnTarget.MAGN_TARGET_40DB,
    "42dB": MagnTarget.MAGN_TARGET_42DB,
}

MaxLnaGain = ns.enum("MaxLnaGain", True)
MAX_LNA_GAIN = {
    "Default": MaxLnaGain.MAX_LNA_GAIN_DEFAULT,
    "2.6dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_2P6DB,
    "6.1dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_6P1DB,
    "7.4dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_7P4DB,
    "9.2dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_9P2DB,
    "11.5dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_11P5DB,
    "14.6dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_14P6DB,
    "17.1dB": MaxLnaGain.MAX_LNA_GAIN_MINUS_17P1DB,
}

MaxDvgaGain = ns.enum("MaxDvgaGain", True)
MAX_DVGA_GAIN = {
    "Default": MaxDvgaGain.MAX_DVGA_GAIN_DEFAULT,
    "-1": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_1,
    "-2": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_2,
    "-3": MaxDvgaGain.MAX_DVGA_GAIN_MINUS_3,
}

CarrierSenseRelThr = ns.enum("CarrierSenseRelThr", True)
CARRIER_SENSE_REL_THR = {
    "Default": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_DEFAULT,
    "+6dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_6DB,
    "+10dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_10DB,
    "+14dB": CarrierSenseRelThr.CARRIER_SENSE_REL_THR_PLUS_14DB,
}

FilterLengthFskMsk = ns.enum("FilterLengthFskMsk", True)
FILTER_LENGTH_FSK_MSK = {
    "8": FilterLengthFskMsk.FILTER_LENGTH_8DB,
    "16": FilterLengthFskMsk.FILTER_LENGTH_16DB,
    "32": FilterLengthFskMsk.FILTER_LENGTH_32DB,
    "64": FilterLengthFskMsk.FILTER_LENGTH_64DB,
}

FilterLengthAskOok = ns.enum("FilterLengthAskOok", True)
FILTER_LENGTH_ASK_OOK = {
    "4dB": FilterLengthAskOok.FILTER_LENGTH_4DB,
    "8dB": FilterLengthAskOok.FILTER_LENGTH_8DB,
    "12dB": FilterLengthAskOok.FILTER_LENGTH_12DB,
    "16dB": FilterLengthAskOok.FILTER_LENGTH_16DB,
}

Freeze = ns.enum("Freeze", True)
FREEZE = {
    "Default": Freeze.FREEZE_DEFAULT,
    "On Sync": Freeze.FREEZE_ON_SYNC,
    "Analog Only": Freeze.FREEZE_ANALOG_ONLY,
    "Analog And Digital": Freeze.FREEZE_ANALOG_AND_DIGITAL,
}

WaitTime = ns.enum("WaitTime", True)
WAIT_TIME = {
    "8": WaitTime.WAIT_TIME_8_SAMPLES,
    "16": WaitTime.WAIT_TIME_16_SAMPLES,
    "24": WaitTime.WAIT_TIME_24_SAMPLES,
    "32": WaitTime.WAIT_TIME_32_SAMPLES,
}

HystLevel = ns.enum("HystLevel", True)
HYST_LEVEL = {
    "None": HystLevel.HYST_LEVEL_NONE,
    "Low": HystLevel.HYST_LEVEL_LOW,
    "Medium": HystLevel.HYST_LEVEL_MEDIUM,
    "High": HystLevel.HYST_LEVEL_HIGH,
}

# Optional settings to generate setter calls for
CONFIG_MAP = {
    cv.Optional(CONF_OUTPUT_POWER, default=10): cv.float_range(min=-30.0, max=11.0),
    cv.Optional(CONF_RX_ATTENUATION, default="0dB"): cv.enum(
        RX_ATTENUATION, upper=False
    ),
    cv.Optional(CONF_DC_BLOCKING_FILTER, default=True): cv.boolean,
    cv.Optional(CONF_FREQUENCY, default="433.92MHz"): cv.All(
        cv.frequency, cv.float_range(min=300.0e6, max=928.0e6)
    ),
    cv.Optional(CONF_IF_FREQUENCY, default="153kHz"): cv.All(
        cv.frequency, cv.float_range(min=25000, max=788000)
    ),
    cv.Optional(CONF_FILTER_BANDWIDTH, default="203kHz"): cv.All(
        cv.frequency, cv.float_range(min=58000, max=812000)
    ),
    cv.Optional(CONF_CHANNEL, default=0): cv.uint8_t,
    cv.Optional(CONF_CHANNEL_SPACING, default="200kHz"): cv.All(
        cv.frequency, cv.float_range(min=25000, max=405000)
    ),
    cv.Optional(CONF_FSK_DEVIATION): cv.All(
        cv.frequency, cv.float_range(min=1500, max=381000)
    ),
    cv.Optional(CONF_MSK_DEVIATION): cv.int_range(min=1, max=8),
    cv.Optional(CONF_SYMBOL_RATE, default=5000): cv.float_range(min=600, max=500000),
    cv.Optional(CONF_SYNC_MODE, default="16/16"): cv.enum(SYNC_MODE, upper=False),
    cv.Optional(CONF_CARRIER_SENSE_ABOVE_THRESHOLD, default=False): cv.boolean,
    cv.Optional(CONF_MODULATION_TYPE, default="ASK/OOK"): cv.enum(
        MODULATION, upper=False
    ),
    cv.Optional(CONF_MANCHESTER, default=False): cv.boolean,
    cv.Optional(CONF_NUM_PREAMBLE, default=2): cv.int_range(min=0, max=7),
    cv.Optional(CONF_SYNC1, default=0xD3): cv.hex_uint8_t,
    cv.Optional(CONF_SYNC0, default=0x91): cv.hex_uint8_t,
    cv.Optional(CONF_MAGN_TARGET, default="42dB"): cv.enum(MAGN_TARGET, upper=False),
    cv.Optional(CONF_MAX_LNA_GAIN, default="Default"): cv.enum(
        MAX_LNA_GAIN, upper=False
    ),
    cv.Optional(CONF_MAX_DVGA_GAIN, default="-3"): cv.enum(MAX_DVGA_GAIN, upper=False),
    cv.Optional(CONF_CARRIER_SENSE_ABS_THR): cv.int_range(min=-8, max=7),
    cv.Optional(CONF_CARRIER_SENSE_REL_THR): cv.enum(
        CARRIER_SENSE_REL_THR, upper=False
    ),
    cv.Optional(CONF_LNA_PRIORITY, default=False): cv.boolean,
    cv.Optional(CONF_FILTER_LENGTH_FSK_MSK): cv.enum(
        FILTER_LENGTH_FSK_MSK, upper=False
    ),
    cv.Optional(CONF_FILTER_LENGTH_ASK_OOK): cv.enum(
        FILTER_LENGTH_ASK_OOK, upper=False
    ),
    cv.Optional(CONF_FREEZE): cv.enum(FREEZE, upper=False),
    cv.Optional(CONF_WAIT_TIME, default="32"): cv.enum(WAIT_TIME, upper=False),
    cv.Optional(CONF_HYST_LEVEL): cv.enum(HYST_LEVEL, upper=False),
    cv.Optional(CONF_PACKET_MODE, default=False): cv.boolean,
    cv.Optional(CONF_PACKET_LENGTH): cv.uint8_t,
    cv.Optional(CONF_CRC_ENABLE, default=False): cv.boolean,
    cv.Optional(CONF_WHITENING, default=False): cv.boolean,
}


def _validate_packet_mode(config):
    if config.get(CONF_PACKET_MODE, False):
        if CONF_GDO0_PIN not in config:
            raise cv.Invalid("gdo0_pin is required when packet_mode is enabled")
        if CONF_PACKET_LENGTH not in config:
            raise cv.Invalid("packet_length is required when packet_mode is enabled")
        if config[CONF_PACKET_LENGTH] > 64:
            raise cv.Invalid("packet_length must be <= 64 (FIFO size)")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CC1101Component),
            cv.Optional(CONF_GDO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(single=True),
        }
    )
    .extend(CONFIG_MAP)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True)),
    _validate_packet_mode,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    for opt in CONFIG_MAP:
        key = opt.schema
        if key in config:
            cg.add(getattr(var, f"set_{key}")(config[key]))

    if CONF_GDO0_PIN in config:
        gdo0_pin = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
        cg.add(var.set_gdo0_pin(gdo0_pin))
    if CONF_ON_PACKET in config:
        await automation.build_automation(
            var.get_packet_trigger(),
            [
                (cg.std_vector.template(cg.uint8), "x"),
                (cg.float_, "rssi"),
                (cg.uint8, "lqi"),
            ],
            config[CONF_ON_PACKET],
        )


# Actions
BeginTxAction = ns.class_("BeginTxAction", automation.Action)
BeginRxAction = ns.class_("BeginRxAction", automation.Action)
ResetAction = ns.class_("ResetAction", automation.Action)
SetIdleAction = ns.class_("SetIdleAction", automation.Action)
SendPacketAction = ns.class_(
    "SendPacketAction", automation.Action, cg.Parented.template(CC1101Component)
)

CC1101_ACTION_SCHEMA = cv.Schema(
    maybe_simple_id({cv.GenerateID(CONF_ID): cv.use_id(CC1101Component)})
)


@automation.register_action("cc1101.begin_tx", BeginTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.begin_rx", BeginRxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.reset", ResetAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.set_idle", SetIdleAction, CC1101_ACTION_SCHEMA)
async def cc1101_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


SEND_PACKET_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(CC1101Component),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    },
    key=CONF_DATA,
)


@automation.register_action(
    "cc1101.send_packet", SendPacketAction, SEND_PACKET_ACTION_SCHEMA
)
async def send_packet_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        # Generate static array in flash to avoid RAM copy
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var
