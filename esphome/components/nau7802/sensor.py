from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_GAIN, CONF_ID, ICON_SCALE, STATE_CLASS_MEASUREMENT

CODEOWNERS = ["@cujomalainey"]
DEPENDENCIES = ["i2c"]

CONF_GAIN_CALIBRATION = "gain_calibration"
CONF_OFFSET_CALIBRATION = "offset_calibration"
CONF_LDO_VOLTAGE = "ldo_voltage"
CONF_SAMPLES_PER_SECOND = "samples_per_second"

nau7802_ns = cg.esphome_ns.namespace("nau7802")
NAU7802Sensor = nau7802_ns.class_(
    "NAU7802Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)
NAU7802CalbrateExternalOffsetAction = nau7802_ns.class_(
    "NAU7802CalbrateExternalOffsetAction",
    automation.Action,
    cg.Parented.template(NAU7802Sensor),
)
NAU7802CalbrateInternalOffsetAction = nau7802_ns.class_(
    "NAU7802CalbrateInternalOffsetAction",
    automation.Action,
    cg.Parented.template(NAU7802Sensor),
)
NAU7802CalbrateGainAction = nau7802_ns.class_(
    "NAU7802CalbrateGainAction", automation.Action, cg.Parented.template(NAU7802Sensor)
)

NAU7802Gain = nau7802_ns.enum("NAU7802Gain")
GAINS = {
    128: NAU7802Gain.NAU7802_GAIN_128,
    64: NAU7802Gain.NAU7802_GAIN_64,
    32: NAU7802Gain.NAU7802_GAIN_32,
    16: NAU7802Gain.NAU7802_GAIN_16,
    8: NAU7802Gain.NAU7802_GAIN_8,
    4: NAU7802Gain.NAU7802_GAIN_4,
    2: NAU7802Gain.NAU7802_GAIN_2,
    1: NAU7802Gain.NAU7802_GAIN_1,
}

NAU7802SPS = nau7802_ns.enum("NAU7802SPS")
SAMPLES_PER_SECOND = {
    320: NAU7802SPS.NAU7802_SPS_320,
    80: NAU7802SPS.NAU7802_SPS_80,
    40: NAU7802SPS.NAU7802_SPS_40,
    20: NAU7802SPS.NAU7802_SPS_20,
    10: NAU7802SPS.NAU7802_SPS_10,
}

NAU7802LDO = nau7802_ns.enum("NAU7802LDO")
LDO = {
    "2.4V": NAU7802LDO.NAU7802_LDO_2V4,
    "2.7V": NAU7802LDO.NAU7802_LDO_2V7,
    "3.0V": NAU7802LDO.NAU7802_LDO_3V0,
    "3.3V": NAU7802LDO.NAU7802_LDO_3V3,
    "3.6V": NAU7802LDO.NAU7802_LDO_3V6,
    "3.9V": NAU7802LDO.NAU7802_LDO_3V9,
    "4.2V": NAU7802LDO.NAU7802_LDO_4V2,
    "4.5V": NAU7802LDO.NAU7802_LDO_4V5,
    "EXTERNAL": NAU7802LDO.NAU7802_LDO_EXTERNAL,
    "EXT": NAU7802LDO.NAU7802_LDO_EXTERNAL,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        NAU7802Sensor,
        icon=ICON_SCALE,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_LDO_VOLTAGE, default="3.0V"): cv.enum(LDO, upper=True),
            cv.Optional(CONF_SAMPLES_PER_SECOND, default=10): cv.enum(
                SAMPLES_PER_SECOND, int=True
            ),
            cv.Optional(CONF_GAIN, default=128): cv.enum(GAINS, int=True),
            cv.Optional(CONF_OFFSET_CALIBRATION, default=0): cv.int_range(
                min=-8388608, max=8388607
            ),
            cv.Optional(CONF_GAIN_CALIBRATION, default=1.0): cv.float_range(
                min=0, max=511.9999998807907
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x2A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_samples_per_second(config[CONF_SAMPLES_PER_SECOND]))
    cg.add(var.set_ldo_voltage(config[CONF_LDO_VOLTAGE]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_gain_calibration(config[CONF_GAIN_CALIBRATION]))
    cg.add(var.set_offset_calibration(config[CONF_OFFSET_CALIBRATION]))


NAU7802_CALIBRATE_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(NAU7802Sensor),
    }
)


@automation.register_action(
    "nau7802.calibrate_internal_offset",
    NAU7802CalbrateInternalOffsetAction,
    NAU7802_CALIBRATE_SCHEMA,
)
@automation.register_action(
    "nau7802.calibrate_external_offset",
    NAU7802CalbrateExternalOffsetAction,
    NAU7802_CALIBRATE_SCHEMA,
)
@automation.register_action(
    "nau7802.calibrate_gain",
    NAU7802CalbrateGainAction,
    NAU7802_CALIBRATE_SCHEMA,
)
async def nau7802_calibrate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
