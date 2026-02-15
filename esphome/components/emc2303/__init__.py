import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID

CODEOWNERS = ["@LinoSchmidt"]

DEPENDENCIES = ["i2c"]

MULTI_CONF = True

CONF_WATCHDOG = "watchdog"
CONF_FAN_1 = "fan_1"
CONF_FAN_2 = "fan_2"
CONF_FAN_3 = "fan_3"
CONF_PULSES_PER_REVOLUTION = "pulses_per_revolution"
CONF_MIN_SPEED_MEASUREMENT = "min_speed_measurement"
CONF_FAN = "fan"

CONF_EMC2303_ID = "emc2303_id"

emc2303_ns = cg.esphome_ns.namespace("emc2303")
Emc2303Component = emc2303_ns.class_("Emc2303Component", cg.Component, i2c.I2CDevice)

Emc2303PwmFrequency = emc2303_ns.enum("Emc2303PwmFrequency")
PWM_FREQUENCY = {
    "26kHz": Emc2303PwmFrequency.EMC2303_PWM_FREQUENCY_26000HZ,
    "19.5kHz": Emc2303PwmFrequency.EMC2303_PWM_FREQUENCY_19531HZ,
    "4.9kHz": Emc2303PwmFrequency.EMC2303_PWM_FREQUENCY_4882HZ,
    "2.4kHz": Emc2303PwmFrequency.EMC2303_PWM_FREQUENCY_2441HZ,
}

Emc2303MinSpeedMeasurement = emc2303_ns.enum("Emc2303MinSpeedMeasurement")
MIN_SPEED_MEASUREMENT = {
    "500RPM": Emc2303MinSpeedMeasurement.EMC2303_MIN_SPEED_500RPM,
    "1000RPM": Emc2303MinSpeedMeasurement.EMC2303_MIN_SPEED_1000RPM,
    "2000RPM": Emc2303MinSpeedMeasurement.EMC2303_MIN_SPEED_2000RPM,
    "4000RPM": Emc2303MinSpeedMeasurement.EMC2303_MIN_SPEED_4000RPM,
}

EMC2303_FAN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FREQUENCY, default="26kHz"): cv.enum(PWM_FREQUENCY),
        cv.Optional(CONF_PULSES_PER_REVOLUTION, default=2): cv.int_range(min=1, max=4),
        cv.Optional(CONF_MIN_SPEED_MEASUREMENT, default="500RPM"): cv.enum(
            MIN_SPEED_MEASUREMENT
        ),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Emc2303Component),
            cv.Optional(CONF_WATCHDOG, default=False): cv.boolean,
            cv.Optional(CONF_FAN_1, default={}): EMC2303_FAN_SCHEMA,
            cv.Optional(CONF_FAN_2, default={}): EMC2303_FAN_SCHEMA,
            cv.Optional(CONF_FAN_3, default={}): EMC2303_FAN_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x2F))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_watchdog(config[CONF_WATCHDOG]))
    for fan in [CONF_FAN_1, CONF_FAN_2, CONF_FAN_3]:
        cg.add(var.set_pwm_frequency(int(fan[-1]), config[fan][CONF_FREQUENCY]))
        cg.add(
            var.set_pulses_per_revolution(
                int(fan[-1]), config[fan][CONF_PULSES_PER_REVOLUTION]
            )
        )
        cg.add(
            var.set_min_speed_measurement(
                int(fan[-1]), config[fan][CONF_MIN_SPEED_MEASUREMENT]
            )
        )
