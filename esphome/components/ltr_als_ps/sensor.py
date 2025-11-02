from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTUAL_GAIN,
    CONF_ACTUAL_INTEGRATION_TIME,
    CONF_AMBIENT_LIGHT,
    CONF_AUTO_MODE,
    CONF_FULL_SPECTRUM_COUNTS,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_INTEGRATION_TIME,
    CONF_NAME,
    CONF_REPEAT,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    ICON_BRIGHTNESS_6,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
    UNIT_MILLISECOND,
)

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

CONF_INFRARED_COUNTS = "infrared_counts"
CONF_ON_PS_HIGH_THRESHOLD = "on_ps_high_threshold"
CONF_ON_PS_LOW_THRESHOLD = "on_ps_low_threshold"
CONF_PS_COOLDOWN = "ps_cooldown"
CONF_PS_COUNTS = "ps_counts"
CONF_PS_GAIN = "ps_gain"
CONF_PS_HIGH_THRESHOLD = "ps_high_threshold"
CONF_PS_LOW_THRESHOLD = "ps_low_threshold"
ICON_BRIGHTNESS_7 = "mdi:brightness-7"
ICON_GAIN = "mdi:multiplication"
ICON_PROXIMITY = "mdi:hand-wave-outline"
UNIT_COUNTS = "#"

ltr_als_ps_ns = cg.esphome_ns.namespace("ltr_als_ps")

LTRAlsPsComponent = ltr_als_ps_ns.class_(
    "LTRAlsPsComponent", cg.PollingComponent, i2c.I2CDevice
)

LtrType = ltr_als_ps_ns.enum("LtrType")
LTR_TYPES = {
    "ALS": LtrType.LTR_TYPE_ALS_ONLY,
    "PS": LtrType.LTR_TYPE_PS_ONLY,
    "ALS_PS": LtrType.LTR_TYPE_ALS_AND_PS,
}

AlsGain = ltr_als_ps_ns.enum("AlsGain")
ALS_GAINS = {
    "1X": AlsGain.GAIN_1,
    "2X": AlsGain.GAIN_2,
    "4X": AlsGain.GAIN_4,
    "8X": AlsGain.GAIN_8,
    "48X": AlsGain.GAIN_48,
    "96X": AlsGain.GAIN_96,
}

IntegrationTime = ltr_als_ps_ns.enum("IntegrationTime")
INTEGRATION_TIMES = {
    50: IntegrationTime.INTEGRATION_TIME_50MS,
    100: IntegrationTime.INTEGRATION_TIME_100MS,
    150: IntegrationTime.INTEGRATION_TIME_150MS,
    200: IntegrationTime.INTEGRATION_TIME_200MS,
    250: IntegrationTime.INTEGRATION_TIME_250MS,
    300: IntegrationTime.INTEGRATION_TIME_300MS,
    350: IntegrationTime.INTEGRATION_TIME_350MS,
    400: IntegrationTime.INTEGRATION_TIME_400MS,
}

MeasurementRepeatRate = ltr_als_ps_ns.enum("MeasurementRepeatRate")
MEASUREMENT_REPEAT_RATES = {
    50: MeasurementRepeatRate.REPEAT_RATE_50MS,
    100: MeasurementRepeatRate.REPEAT_RATE_100MS,
    200: MeasurementRepeatRate.REPEAT_RATE_200MS,
    500: MeasurementRepeatRate.REPEAT_RATE_500MS,
    1000: MeasurementRepeatRate.REPEAT_RATE_1000MS,
    2000: MeasurementRepeatRate.REPEAT_RATE_2000MS,
}

PsGain = ltr_als_ps_ns.enum("PsGain")
PS_GAINS = {
    "16X": PsGain.PS_GAIN_16,
    "32X": PsGain.PS_GAIN_32,
    "64X": PsGain.PS_GAIN_64,
}

LTRPsHighTrigger = ltr_als_ps_ns.class_(
    "LTRPsHighTrigger", automation.Trigger.template()
)
LTRPsLowTrigger = ltr_als_ps_ns.class_("LTRPsLowTrigger", automation.Trigger.template())


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(INTEGRATION_TIMES, int=True)(value)


def validate_repeat_rate(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(MEASUREMENT_REPEAT_RATES, int=True)(value)


def validate_time_and_repeat_rate(config):
    integraton_time = config[CONF_INTEGRATION_TIME]
    repeat_rate = config[CONF_REPEAT]
    if integraton_time > repeat_rate:
        raise cv.Invalid(
            f"Measurement repeat rate ({repeat_rate}ms) shall be greater or equal to integration time ({integraton_time}ms)"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LTRAlsPsComponent),
            cv.Optional(CONF_TYPE, default="ALS_PS"): cv.enum(LTR_TYPES, upper=True),
            cv.Optional(CONF_AUTO_MODE, default=True): cv.boolean,
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(ALS_GAINS, upper=True),
            cv.Optional(
                CONF_INTEGRATION_TIME, default="100ms"
            ): validate_integration_time,
            cv.Optional(CONF_REPEAT, default="500ms"): validate_repeat_rate,
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
            cv.Optional(
                CONF_PS_COOLDOWN, default="5s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_PS_GAIN, default="16X"): cv.enum(PS_GAINS, upper=True),
            cv.Optional(CONF_PS_HIGH_THRESHOLD, default=65535): cv.int_range(
                min=0, max=65535
            ),
            cv.Optional(CONF_PS_LOW_THRESHOLD, default=0): cv.int_range(
                min=0, max=65535
            ),
            cv.Optional(CONF_ON_PS_HIGH_THRESHOLD): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LTRPsHighTrigger),
                }
            ),
            cv.Optional(CONF_ON_PS_LOW_THRESHOLD): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LTRPsLowTrigger),
                }
            ),
            cv.Optional(CONF_AMBIENT_LIGHT): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_LUX,
                    icon=ICON_BRIGHTNESS_6,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_INFRARED_COUNTS): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_COUNTS,
                    icon=ICON_BRIGHTNESS_5,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_FULL_SPECTRUM_COUNTS): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_COUNTS,
                    icon=ICON_BRIGHTNESS_7,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PS_COUNTS): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_COUNTS,
                    icon=ICON_PROXIMITY,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_DISTANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACTUAL_GAIN): cv.maybe_simple_value(
                sensor.sensor_schema(
                    icon=ICON_GAIN,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ACTUAL_INTEGRATION_TIME): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_MILLISECOND,
                    icon=ICON_TIMER,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29)),
    validate_time_and_repeat_rate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if als_config := config.get(CONF_AMBIENT_LIGHT):
        sens = await sensor.new_sensor(als_config)
        cg.add(var.set_ambient_light_sensor(sens))

    if infrared_cnt_config := config.get(CONF_INFRARED_COUNTS):
        sens = await sensor.new_sensor(infrared_cnt_config)
        cg.add(var.set_infrared_counts_sensor(sens))

    if full_spect_cnt_config := config.get(CONF_FULL_SPECTRUM_COUNTS):
        sens = await sensor.new_sensor(full_spect_cnt_config)
        cg.add(var.set_full_spectrum_counts_sensor(sens))

    if act_gain_config := config.get(CONF_ACTUAL_GAIN):
        sens = await sensor.new_sensor(act_gain_config)
        cg.add(var.set_actual_gain_sensor(sens))

    if act_itime_config := config.get(CONF_ACTUAL_INTEGRATION_TIME):
        sens = await sensor.new_sensor(act_itime_config)
        cg.add(var.set_actual_integration_time_sensor(sens))

    if prox_cnt_config := config.get(CONF_PS_COUNTS):
        sens = await sensor.new_sensor(prox_cnt_config)
        cg.add(var.set_proximity_counts_sensor(sens))

    for prox_high_tr in config.get(CONF_ON_PS_HIGH_THRESHOLD, []):
        trigger = cg.new_Pvariable(prox_high_tr[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], prox_high_tr)

    for prox_low_tr in config.get(CONF_ON_PS_LOW_THRESHOLD, []):
        trigger = cg.new_Pvariable(prox_low_tr[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], prox_low_tr)

    cg.add(var.set_ltr_type(config[CONF_TYPE]))

    cg.add(var.set_als_auto_mode(config[CONF_AUTO_MODE]))
    cg.add(var.set_als_gain(config[CONF_GAIN]))
    cg.add(var.set_als_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_als_meas_repeat_rate(config[CONF_REPEAT]))
    cg.add(var.set_als_glass_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))

    cg.add(var.set_ps_cooldown_time_s(config[CONF_PS_COOLDOWN]))
    cg.add(var.set_ps_gain(config[CONF_PS_GAIN]))
    cg.add(var.set_ps_high_threshold(config[CONF_PS_HIGH_THRESHOLD]))
    cg.add(var.set_ps_low_threshold(config[CONF_PS_LOW_THRESHOLD]))
