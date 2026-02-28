import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@LinoSchmidt"]

DEPENDENCIES = ["i2c"]

MULTI_CONF = True

CONF_WATCHDOG = "watchdog"
CONF_FAN_1 = "fan_1"
CONF_FAN_2 = "fan_2"
CONF_FAN_3 = "fan_3"
CONF_FAN_4 = "fan_4"
CONF_FAN_5 = "fan_5"
CONF_PWM_PUSH_PULL = "pwm_push_pull"
CONF_PWM_FREQUENCY = "pwm_frequency"
CONF_PWM_DIVIDER = "pwm_divider"
CONF_PULSES_PER_REVOLUTION = "pulses_per_revolution"
CONF_MIN_SPEED_MEASUREMENT = "min_speed_measurement"
CONF_UPDATE_TIME = "update_time"
CONF_MAX_STEP_SIZE = "max_step_size"
CONF_NOISE_FILTER = "noise_filter"
CONF_SPIN_UP_KICK = "spin_up_kick"
CONF_SPIN_UP_LEVEL = "spin_up_level"
CONF_SPIN_UP_TIME = "spin_up_time"
CONF_FAN = "fan"

CONF_EMC230X_ID = "emc230x_id"

emc230x_ns = cg.esphome_ns.namespace("emc230x")
Emc230xComponent = emc230x_ns.class_("Emc230xComponent", cg.Component, i2c.I2CDevice)

Emc230xPwmFrequency = emc230x_ns.enum("Emc230xPwmFrequency")
PWM_FREQUENCY = {
    "26kHz": Emc230xPwmFrequency.EMC230X_PWM_FREQUENCY_26000HZ,
    "19.5kHz": Emc230xPwmFrequency.EMC230X_PWM_FREQUENCY_19531HZ,
    "4.9kHz": Emc230xPwmFrequency.EMC230X_PWM_FREQUENCY_4882HZ,
    "2.4kHz": Emc230xPwmFrequency.EMC230X_PWM_FREQUENCY_2441HZ,
}

Emc230xMinSpeedMeasurement = emc230x_ns.enum("Emc230xMinSpeedMeasurement")
MIN_SPEED_MEASUREMENT = {
    "500RPM": Emc230xMinSpeedMeasurement.EMC230X_MIN_SPEED_500RPM,
    "1000RPM": Emc230xMinSpeedMeasurement.EMC230X_MIN_SPEED_1000RPM,
    "2000RPM": Emc230xMinSpeedMeasurement.EMC230X_MIN_SPEED_2000RPM,
    "4000RPM": Emc230xMinSpeedMeasurement.EMC230X_MIN_SPEED_4000RPM,
}

Emc230xUpdateTime = emc230x_ns.enum("Emc230xUpdateTime")
UPDATE_TIME = {
    "100ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_100MS,
    "200ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_200MS,
    "300ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_300MS,
    "400ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_400MS,
    "500ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_500MS,
    "800ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_800MS,
    "1200ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_1200MS,
    "1600ms": Emc230xUpdateTime.EMC230X_UPDATE_TIME_1600MS,
}

Emc230xSpinUpLevel = emc230x_ns.enum("Emc230xSpinUpLevel")
SPIN_UP_LEVEL = {
    "30%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_30,
    "35%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_35,
    "40%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_40,
    "45%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_45,
    "50%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_50,
    "55%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_55,
    "60%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_60,
    "65%": Emc230xSpinUpLevel.EMC230X_SPIN_UP_LEVEL_65,
}

Emc230xSpinUpTime = emc230x_ns.enum("Emc230xSpinUpTime")
SPIN_UP_TIME = {
    "0.25s": Emc230xSpinUpTime.EMC230X_SPIN_UP_TIME_250MS,
    "0.5s": Emc230xSpinUpTime.EMC230X_SPIN_UP_TIME_500MS,
    "1s": Emc230xSpinUpTime.EMC230X_SPIN_UP_TIME_1S,
    "2s": Emc230xSpinUpTime.EMC230X_SPIN_UP_TIME_2S,
}

EMC230X_FAN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PWM_PUSH_PULL, default=False): cv.boolean,
        cv.Optional(CONF_PWM_FREQUENCY, default="26kHz"): cv.enum(PWM_FREQUENCY),
        cv.Optional(CONF_PWM_DIVIDER, default=1): cv.int_range(min=1, max=255),
        cv.Optional(CONF_PULSES_PER_REVOLUTION, default=2): cv.int_range(min=1, max=4),
        cv.Optional(CONF_MIN_SPEED_MEASUREMENT, default="500RPM"): cv.enum(
            MIN_SPEED_MEASUREMENT
        ),
        cv.Optional(CONF_UPDATE_TIME, default="400ms"): cv.enum(UPDATE_TIME),
        cv.Optional(CONF_MAX_STEP_SIZE, default=0): cv.int_range(min=0, max=31),
        cv.Optional(CONF_NOISE_FILTER, default=True): cv.boolean,
        cv.Optional(CONF_SPIN_UP_KICK, default=False): cv.boolean,
        cv.Optional(CONF_SPIN_UP_LEVEL, default="60%"): cv.enum(SPIN_UP_LEVEL),
        cv.Optional(CONF_SPIN_UP_TIME, default="0.5s"): cv.enum(SPIN_UP_TIME),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Emc230xComponent),
            cv.Optional(CONF_WATCHDOG, default=False): cv.boolean,
            cv.Optional(CONF_FAN_1, default={}): EMC230X_FAN_SCHEMA,
            cv.Optional(CONF_FAN_2, default={}): EMC230X_FAN_SCHEMA,
            cv.Optional(CONF_FAN_3, default={}): EMC230X_FAN_SCHEMA,
            cv.Optional(CONF_FAN_4, default={}): EMC230X_FAN_SCHEMA,
            cv.Optional(CONF_FAN_5, default={}): EMC230X_FAN_SCHEMA,
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
    for fan in [CONF_FAN_1, CONF_FAN_2, CONF_FAN_3, CONF_FAN_4, CONF_FAN_5]:
        cg.add(var.set_pwm_push_pull(int(fan[-1]), config[fan][CONF_PWM_PUSH_PULL]))
        cg.add(var.set_pwm_frequency(int(fan[-1]), config[fan][CONF_PWM_FREQUENCY]))
        cg.add(var.set_pwm_divider(int(fan[-1]), config[fan][CONF_PWM_DIVIDER]))
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
        cg.add(var.set_update_time(int(fan[-1]), config[fan][CONF_UPDATE_TIME]))
        cg.add(var.set_max_step_size(int(fan[-1]), config[fan][CONF_MAX_STEP_SIZE]))
        cg.add(var.set_noise_filter(int(fan[-1]), config[fan][CONF_NOISE_FILTER]))
        cg.add(var.set_spin_up_kick(int(fan[-1]), config[fan][CONF_SPIN_UP_KICK]))
        cg.add(var.set_spin_up_level(int(fan[-1]), config[fan][CONF_SPIN_UP_LEVEL]))
        cg.add(var.set_spin_up_time(int(fan[-1]), config[fan][CONF_SPIN_UP_TIME]))
