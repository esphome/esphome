from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi
from esphome.components.const import CONF_CRC_ENABLE, CONF_ON_PACKET
import esphome.config_validation as cv
from esphome.const import CONF_BUSY_PIN, CONF_DATA, CONF_FREQUENCY, CONF_ID
from esphome.core import ID, TimePeriod

MULTI_CONF = True
CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["spi"]

CONF_SX126X_ID = "sx126x_id"

CONF_BANDWIDTH = "bandwidth"
CONF_BITRATE = "bitrate"
CONF_CODING_RATE = "coding_rate"
CONF_CRC_INVERTED = "crc_inverted"
CONF_CRC_SIZE = "crc_size"
CONF_CRC_POLYNOMIAL = "crc_polynomial"
CONF_CRC_INITIAL = "crc_initial"
CONF_DEVIATION = "deviation"
CONF_DIO1_PIN = "dio1_pin"
CONF_HW_VERSION = "hw_version"
CONF_MODULATION = "modulation"
CONF_PA_POWER = "pa_power"
CONF_PA_RAMP = "pa_ramp"
CONF_PAYLOAD_LENGTH = "payload_length"
CONF_PREAMBLE_DETECT = "preamble_detect"
CONF_PREAMBLE_SIZE = "preamble_size"
CONF_RST_PIN = "rst_pin"
CONF_RX_START = "rx_start"
CONF_RF_SWITCH = "rf_switch"
CONF_SHAPING = "shaping"
CONF_SPREADING_FACTOR = "spreading_factor"
CONF_SYNC_VALUE = "sync_value"
CONF_TCXO_VOLTAGE = "tcxo_voltage"
CONF_TCXO_DELAY = "tcxo_delay"

sx126x_ns = cg.esphome_ns.namespace("sx126x")
SX126x = sx126x_ns.class_("SX126x", cg.Component, spi.SPIDevice)
SX126xListener = sx126x_ns.class_("SX126xListener")
SX126xBw = sx126x_ns.enum("SX126xBw")
SX126xPacketType = sx126x_ns.enum("SX126xPacketType")
SX126xTcxoCtrl = sx126x_ns.enum("SX126xTcxoCtrl")
SX126xRampTime = sx126x_ns.enum("SX126xRampTime")
SX126xPulseShape = sx126x_ns.enum("SX126xPulseShape")
SX126xLoraCr = sx126x_ns.enum("SX126xLoraCr")

BW = {
    "4_8kHz": SX126xBw.SX126X_BW_4800,
    "5_8kHz": SX126xBw.SX126X_BW_5800,
    "7_3kHz": SX126xBw.SX126X_BW_7300,
    "9_7kHz": SX126xBw.SX126X_BW_9700,
    "11_7kHz": SX126xBw.SX126X_BW_11700,
    "14_6kHz": SX126xBw.SX126X_BW_14600,
    "19_5kHz": SX126xBw.SX126X_BW_19500,
    "23_4kHz": SX126xBw.SX126X_BW_23400,
    "29_3kHz": SX126xBw.SX126X_BW_29300,
    "39_0kHz": SX126xBw.SX126X_BW_39000,
    "46_9kHz": SX126xBw.SX126X_BW_46900,
    "58_6kHz": SX126xBw.SX126X_BW_58600,
    "78_2kHz": SX126xBw.SX126X_BW_78200,
    "93_8kHz": SX126xBw.SX126X_BW_93800,
    "117_3kHz": SX126xBw.SX126X_BW_117300,
    "156_2kHz": SX126xBw.SX126X_BW_156200,
    "187_2kHz": SX126xBw.SX126X_BW_187200,
    "234_3kHz": SX126xBw.SX126X_BW_234300,
    "312_0kHz": SX126xBw.SX126X_BW_312000,
    "373_6kHz": SX126xBw.SX126X_BW_373600,
    "467_0kHz": SX126xBw.SX126X_BW_467000,
    "7_8kHz": SX126xBw.SX126X_BW_7810,
    "10_4kHz": SX126xBw.SX126X_BW_10420,
    "15_6kHz": SX126xBw.SX126X_BW_15630,
    "20_8kHz": SX126xBw.SX126X_BW_20830,
    "31_3kHz": SX126xBw.SX126X_BW_31250,
    "41_7kHz": SX126xBw.SX126X_BW_41670,
    "62_5kHz": SX126xBw.SX126X_BW_62500,
    "125_0kHz": SX126xBw.SX126X_BW_125000,
    "250_0kHz": SX126xBw.SX126X_BW_250000,
    "500_0kHz": SX126xBw.SX126X_BW_500000,
}

CODING_RATE = {
    "CR_4_5": SX126xLoraCr.LORA_CR_4_5,
    "CR_4_6": SX126xLoraCr.LORA_CR_4_6,
    "CR_4_7": SX126xLoraCr.LORA_CR_4_7,
    "CR_4_8": SX126xLoraCr.LORA_CR_4_8,
}

MOD = {
    "LORA": SX126xPacketType.PACKET_TYPE_LORA,
    "FSK": SX126xPacketType.PACKET_TYPE_GFSK,
}

TCXO_VOLTAGE = {
    "1_6V": SX126xTcxoCtrl.TCXO_CTRL_1_6V,
    "1_7V": SX126xTcxoCtrl.TCXO_CTRL_1_7V,
    "1_8V": SX126xTcxoCtrl.TCXO_CTRL_1_8V,
    "2_2V": SX126xTcxoCtrl.TCXO_CTRL_2_2V,
    "2_4V": SX126xTcxoCtrl.TCXO_CTRL_2_4V,
    "2_7V": SX126xTcxoCtrl.TCXO_CTRL_2_7V,
    "3_0V": SX126xTcxoCtrl.TCXO_CTRL_3_0V,
    "3_3V": SX126xTcxoCtrl.TCXO_CTRL_3_3V,
    "NONE": SX126xTcxoCtrl.TCXO_CTRL_NONE,
}

RAMP = {
    "10us": SX126xRampTime.PA_RAMP_10,
    "20us": SX126xRampTime.PA_RAMP_20,
    "40us": SX126xRampTime.PA_RAMP_40,
    "80us": SX126xRampTime.PA_RAMP_80,
    "200us": SX126xRampTime.PA_RAMP_200,
    "800us": SX126xRampTime.PA_RAMP_800,
    "1700us": SX126xRampTime.PA_RAMP_1700,
    "3400us": SX126xRampTime.PA_RAMP_3400,
}

SHAPING = {
    "GAUSSIAN_BT_0_3": SX126xPulseShape.GAUSSIAN_BT_0_3,
    "GAUSSIAN_BT_0_5": SX126xPulseShape.GAUSSIAN_BT_0_5,
    "GAUSSIAN_BT_0_7": SX126xPulseShape.GAUSSIAN_BT_0_7,
    "GAUSSIAN_BT_1_0": SX126xPulseShape.GAUSSIAN_BT_1_0,
    "NONE": SX126xPulseShape.NO_FILTER,
}

RunImageCalAction = sx126x_ns.class_(
    "RunImageCalAction", automation.Action, cg.Parented.template(SX126x)
)
SendPacketAction = sx126x_ns.class_(
    "SendPacketAction", automation.Action, cg.Parented.template(SX126x)
)
SetModeTxAction = sx126x_ns.class_(
    "SetModeTxAction", automation.Action, cg.Parented.template(SX126x)
)
SetModeRxAction = sx126x_ns.class_(
    "SetModeRxAction", automation.Action, cg.Parented.template(SX126x)
)
SetModeSleepAction = sx126x_ns.class_(
    "SetModeSleepAction", automation.Action, cg.Parented.template(SX126x)
)
SetModeStandbyAction = sx126x_ns.class_(
    "SetModeStandbyAction", automation.Action, cg.Parented.template(SX126x)
)


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


def validate_config(config):
    lora_bws = [
        "7_8kHz",
        "10_4kHz",
        "15_6kHz",
        "20_8kHz",
        "31_3kHz",
        "41_7kHz",
        "62_5kHz",
        "125_0kHz",
        "250_0kHz",
        "500_0kHz",
    ]
    if config[CONF_MODULATION] == "LORA":
        if config[CONF_BANDWIDTH] not in lora_bws:
            raise cv.Invalid(f"{config[CONF_BANDWIDTH]} is not available with LORA")
        if config[CONF_PREAMBLE_SIZE] < 6:
            raise cv.Invalid("Minimum 'preamble_size' is 6 with LORA")
        if config[CONF_SPREADING_FACTOR] == 6 and config[CONF_PAYLOAD_LENGTH] == 0:
            raise cv.Invalid("Payload length must be set when spreading factor is 6")
    else:
        if config[CONF_BANDWIDTH] in lora_bws:
            raise cv.Invalid(f"{config[CONF_BANDWIDTH]} is not available with FSK")
        if config[CONF_PREAMBLE_DETECT] > len(config[CONF_SYNC_VALUE]):
            raise cv.Invalid("Preamble detection length must be <= sync value length")
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SX126x),
            cv.Optional(CONF_BANDWIDTH, default="125_0kHz"): cv.enum(BW),
            cv.Optional(CONF_BITRATE, default=4800): cv.int_range(min=600, max=300000),
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_CODING_RATE, default="CR_4_5"): cv.enum(CODING_RATE),
            cv.Optional(CONF_CRC_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_CRC_INVERTED, default=True): cv.boolean,
            cv.Optional(CONF_CRC_SIZE, default=2): cv.int_range(min=1, max=2),
            cv.Optional(CONF_CRC_POLYNOMIAL, default=0x1021): cv.All(
                cv.hex_int, cv.Range(min=0, max=0xFFFF)
            ),
            cv.Optional(CONF_CRC_INITIAL, default=0x1D0F): cv.All(
                cv.hex_int, cv.Range(min=0, max=0xFFFF)
            ),
            cv.Optional(CONF_DEVIATION, default="5kHz"): cv.All(
                cv.frequency, cv.float_range(min=0, max=100000)
            ),
            cv.Required(CONF_DIO1_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_FREQUENCY): cv.All(
                cv.frequency, cv.float_range(min=137.0e6, max=1020.0e6)
            ),
            cv.Required(CONF_HW_VERSION): cv.one_of(
                "sx1261", "sx1262", "sx1268", "llcc68", lower=True
            ),
            cv.Required(CONF_MODULATION): cv.enum(MOD),
            cv.Optional(CONF_ON_PACKET): automation.validate_automation(single=True),
            cv.Optional(CONF_PA_POWER, default=17): cv.int_range(min=-3, max=22),
            cv.Optional(CONF_PA_RAMP, default="40us"): cv.enum(RAMP),
            cv.Optional(CONF_PAYLOAD_LENGTH, default=0): cv.int_range(min=0, max=256),
            cv.Optional(CONF_PREAMBLE_DETECT, default=2): cv.int_range(min=0, max=4),
            cv.Optional(CONF_PREAMBLE_SIZE, default=8): cv.int_range(min=1, max=65535),
            cv.Required(CONF_RST_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RX_START, default=True): cv.boolean,
            cv.Required(CONF_RF_SWITCH): cv.boolean,
            cv.Optional(CONF_SHAPING, default="NONE"): cv.enum(SHAPING),
            cv.Optional(CONF_SPREADING_FACTOR, default=7): cv.int_range(min=6, max=12),
            cv.Optional(CONF_SYNC_VALUE, default=[]): cv.ensure_list(cv.hex_uint8_t),
            cv.Optional(CONF_TCXO_VOLTAGE, default="NONE"): cv.enum(TCXO_VOLTAGE),
            cv.Optional(CONF_TCXO_DELAY, default="5ms"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=262144000)),
            ),
        },
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(True, 8e6, "mode0"))
    .add_extra(validate_config)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
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
    if CONF_DIO1_PIN in config:
        dio1_pin = await cg.gpio_pin_expression(config[CONF_DIO1_PIN])
        cg.add(var.set_dio1_pin(dio1_pin))
    rst_pin = await cg.gpio_pin_expression(config[CONF_RST_PIN])
    cg.add(var.set_rst_pin(rst_pin))
    busy_pin = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy_pin))
    cg.add(var.set_bandwidth(config[CONF_BANDWIDTH]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_hw_version(config[CONF_HW_VERSION]))
    cg.add(var.set_deviation(config[CONF_DEVIATION]))
    cg.add(var.set_modulation(config[CONF_MODULATION]))
    cg.add(var.set_pa_ramp(config[CONF_PA_RAMP]))
    cg.add(var.set_pa_power(config[CONF_PA_POWER]))
    cg.add(var.set_shaping(config[CONF_SHAPING]))
    cg.add(var.set_bitrate(config[CONF_BITRATE]))
    cg.add(var.set_crc_enable(config[CONF_CRC_ENABLE]))
    cg.add(var.set_crc_inverted(config[CONF_CRC_INVERTED]))
    cg.add(var.set_crc_size(config[CONF_CRC_SIZE]))
    cg.add(var.set_crc_polynomial(config[CONF_CRC_POLYNOMIAL]))
    cg.add(var.set_crc_initial(config[CONF_CRC_INITIAL]))
    cg.add(var.set_payload_length(config[CONF_PAYLOAD_LENGTH]))
    cg.add(var.set_preamble_size(config[CONF_PREAMBLE_SIZE]))
    cg.add(var.set_preamble_detect(config[CONF_PREAMBLE_DETECT]))
    cg.add(var.set_coding_rate(config[CONF_CODING_RATE]))
    cg.add(var.set_spreading_factor(config[CONF_SPREADING_FACTOR]))
    cg.add(var.set_sync_value(config[CONF_SYNC_VALUE]))
    cg.add(var.set_rx_start(config[CONF_RX_START]))
    cg.add(var.set_rf_switch(config[CONF_RF_SWITCH]))
    cg.add(var.set_tcxo_voltage(config[CONF_TCXO_VOLTAGE]))
    cg.add(var.set_tcxo_delay(config[CONF_TCXO_DELAY]))


NO_ARGS_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SX126x),
    }
)


@automation.register_action(
    "sx126x.run_image_cal", RunImageCalAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "sx126x.set_mode_tx", SetModeTxAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "sx126x.set_mode_rx", SetModeRxAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "sx126x.set_mode_sleep", SetModeSleepAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "sx126x.set_mode_standby", SetModeStandbyAction, NO_ARGS_ACTION_SCHEMA
)
async def no_args_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


SEND_PACKET_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(SX126x),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    },
    key=CONF_DATA,
)


@automation.register_action(
    "sx126x.send_packet", SendPacketAction, SEND_PACKET_ACTION_SCHEMA
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
