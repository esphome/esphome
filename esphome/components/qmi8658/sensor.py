from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_GYROSCOPE_X,
    CONF_GYROSCOPE_Y,
    CONF_GYROSCOPE_Z,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_TEMPERATURE,
    CONF_THRESHOLD,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    ICON_GYROSCOPE_X,
    ICON_GYROSCOPE_Y,
    ICON_GYROSCOPE_Z,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_METER_PER_SECOND_SQUARED,
)

DEPENDENCIES = ["i2c"]

CONF_ACCELERATION_LPF_MODE = "acceleration_lpf_mode"
CONF_ACCELERATION_ODR = "acceleration_odr"
CONF_ACCELERATION_RANGE = "acceleration_range"
CONF_BLANKING_TIME = "blanking_time"
CONF_GYROSCOPE_LPF_MODE = "gyroscope_lpf_mode"
CONF_GYROSCOPE_ODR = "gyroscope_odr"
CONF_GYROSCOPE_RANGE = "gyroscope_range"
CONF_INITIAL_PIN_STATE = "initial_pin_state"

qmi8658_ns = cg.esphome_ns.namespace("qmi8658")
QMI8658Component = qmi8658_ns.class_(
    "QMI8658Component", cg.PollingComponent, i2c.I2CDevice
)

QMI8658InterruptPin = qmi8658_ns.enum("QMI8658InterruptPin")
QMI8658_INTERRUPT_PIN = {
    "INT1": QMI8658InterruptPin.QMI8658_INT1,
    "INT2": QMI8658InterruptPin.QMI8658_INT2,
}

QMI8658LPFMode = qmi8658_ns.enum("QMI8658LPFMode")
QMI8658_LPF_MODE = {
    "OFF": QMI8658LPFMode.QMI8658_LPF_OFF,
    "MODE_0": QMI8658LPFMode.QMI8658_LPF_MODE_0,
    "2.62%": QMI8658LPFMode.QMI8658_LPF_MODE_0,
    "MODE_1": QMI8658LPFMode.QMI8658_LPF_MODE_1,
    "3.59%": QMI8658LPFMode.QMI8658_LPF_MODE_1,
    "MODE_2": QMI8658LPFMode.QMI8658_LPF_MODE_2,
    "5.32%": QMI8658LPFMode.QMI8658_LPF_MODE_2,
    "MODE_3": QMI8658LPFMode.QMI8658_LPF_MODE_3,
    "14%": QMI8658LPFMode.QMI8658_LPF_MODE_3,
}

QMI8658AccelODR = qmi8658_ns.enum("QMI8658AccelODR")
QMI8658_ACCEL_ODR = {
    "8000HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_8000HZ,
    "4000HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_4000HZ,
    "2000HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_2000HZ,
    "1000HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_1000HZ,
    "500HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_500HZ,
    "250HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_250HZ,
    "125HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_125HZ,
    "62.5HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_62_5HZ,
    "31.25HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_31_25HZ,
    "LOWPOWER_128HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_LOWPOWER_128HZ,
    "LOWPOWER_21HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_LOWPOWER_21HZ,
    "LOWPOWER_11HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_LOWPOWER_11HZ,
    "LOWPOWER_3HZ": QMI8658AccelODR.QMI8658_ACCEL_ODR_LOWPOWER_3HZ,
}

QMI8658AccelRange = qmi8658_ns.enum("QMI8658AccelRange")
QMI8658_ACCEL_RANGE = {
    "2G": QMI8658AccelRange.QMI8658_ACCEL_RANGE_2G,
    "4G": QMI8658AccelRange.QMI8658_ACCEL_RANGE_4G,
    "8G": QMI8658AccelRange.QMI8658_ACCEL_RANGE_8G,
    "16G": QMI8658AccelRange.QMI8658_ACCEL_RANGE_16G,
}

QMI8658GyroODR = qmi8658_ns.enum("QMI8658GyroODR")
QMI8658_GYRO_ODR = {
    "8000HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_8000HZ,
    "4000HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_4000HZ,
    "2000HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_2000HZ,
    "1000HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_1000HZ,
    "500HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_500HZ,
    "250HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_250HZ,
    "125HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_125HZ,
    "62.5HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_62_5HZ,
    "31.25HZ": QMI8658GyroODR.QMI8658_GYRO_ODR_31_25HZ,
}

QMI8658GyroRange = qmi8658_ns.enum("QMI8658GyroRange")
QMI8658_GYRO_RANGE = {
    "16DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_16DPS,
    "32DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_32DPS,
    "64DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_64DPS,
    "128DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_128DPS,
    "256DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_256DPS,
    "512DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_512DPS,
    "1024DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_1024DPS,
    "2048DPS": QMI8658GyroRange.QMI8658_GYRO_RANGE_2048DPS,
}

EnableWakeOnMotionAction = qmi8658_ns.class_(
    "EnableWakeOnMotionAction", automation.Action
)
DisableWakeOnMotionAction = qmi8658_ns.class_(
    "DisableWakeOnMotionAction", automation.Action
)

ACCEL_SCHEMA = {
    "unit_of_measurement": UNIT_METER_PER_SECOND_SQUARED,
    "accuracy_decimals": 2,
    "state_class": STATE_CLASS_MEASUREMENT,
}
GYRO_SCHEMA = {
    "unit_of_measurement": UNIT_DEGREE_PER_SECOND,
    "accuracy_decimals": 2,
    "state_class": STATE_CLASS_MEASUREMENT,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QMI8658Component),
            cv.Optional(CONF_ACCELERATION_LPF_MODE, "OFF"): cv.enum(
                QMI8658_LPF_MODE, upper=True
            ),
            cv.Optional(CONF_ACCELERATION_ODR, default="8000HZ"): cv.enum(
                QMI8658_ACCEL_ODR, upper=True
            ),
            cv.Optional(CONF_ACCELERATION_RANGE, default="2G"): cv.enum(
                QMI8658_ACCEL_RANGE, upper=True
            ),
            cv.Optional(CONF_ACCELERATION_X): sensor.sensor_schema(
                icon=ICON_ACCELERATION_X, **ACCEL_SCHEMA
            ),
            cv.Optional(CONF_ACCELERATION_Y): sensor.sensor_schema(
                icon=ICON_ACCELERATION_Y, **ACCEL_SCHEMA
            ),
            cv.Optional(CONF_ACCELERATION_Z): sensor.sensor_schema(
                icon=ICON_ACCELERATION_Z, **ACCEL_SCHEMA
            ),
            cv.Optional(CONF_GYROSCOPE_LPF_MODE, "OFF"): cv.enum(
                QMI8658_LPF_MODE, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_ODR, default="8000HZ"): cv.enum(
                QMI8658_GYRO_ODR, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_RANGE, default="16DPS"): cv.enum(
                QMI8658_GYRO_RANGE, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_X): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_X, **GYRO_SCHEMA
            ),
            cv.Optional(CONF_GYROSCOPE_Y): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_Y, **GYRO_SCHEMA
            ),
            cv.Optional(CONF_GYROSCOPE_Z): sensor.sensor_schema(
                icon=ICON_GYROSCOPE_Z, **GYRO_SCHEMA
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)


ENABLE_WAKE_ON_MOTION_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(QMI8658Component),
        cv.Optional(CONF_ACCELERATION_ODR, default="LOWPOWER_21HZ"): cv.templatable(
            cv.enum(QMI8658_ACCEL_ODR, upper=True)
        ),
        cv.Optional(CONF_BLANKING_TIME, default=0): cv.templatable(
            cv.int_range(min=0, max=63)
        ),
        cv.Required(CONF_INITIAL_PIN_STATE): cv.templatable(cv.int_range(min=0, max=1)),
        cv.Required(CONF_INTERRUPT_PIN): cv.templatable(
            cv.enum(QMI8658_INTERRUPT_PIN, upper=True)
        ),
        cv.Required(CONF_THRESHOLD): cv.templatable(cv.int_range(min=1, max=255)),
    },
)

DISABLE_WAKE_ON_MOTION_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(QMI8658Component),
    },
)


@automation.register_action(
    "qmi8658.enable_wake_on_motion",
    EnableWakeOnMotionAction,
    ENABLE_WAKE_ON_MOTION_ACTION_SCHEMA,
)
async def qmi8658_enable_wake_on_motion_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    accel_odr = await cg.templatable(
        config[CONF_ACCELERATION_ODR], args, QMI8658AccelODR
    )
    blanking_time = await cg.templatable(config[CONF_BLANKING_TIME], args, int)
    initial_pin_state = await cg.templatable(config[CONF_INITIAL_PIN_STATE], args, int)
    interrupt_pin = await cg.templatable(
        config[CONF_INTERRUPT_PIN], args, QMI8658InterruptPin
    )
    threshold = await cg.templatable(config[CONF_THRESHOLD], args, int)
    cg.add(var.set_accel_odr(accel_odr))
    cg.add(var.set_blanking_time(blanking_time))
    cg.add(var.set_initial_pin_state(initial_pin_state))
    cg.add(var.set_interrupt_pin(interrupt_pin))
    cg.add(var.set_threshold(threshold))

    return var


@automation.register_action(
    "qmi8658.disable_wake_on_motion",
    DisableWakeOnMotionAction,
    DISABLE_WAKE_ON_MOTION_ACTION_SCHEMA,
)
async def qmi8658_disable_wake_on_motion_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_accel_lpf_mode(config[CONF_ACCELERATION_LPF_MODE]))
    cg.add(var.set_accel_odr(config[CONF_ACCELERATION_ODR]))
    cg.add(var.set_accel_range(config[CONF_ACCELERATION_RANGE]))
    cg.add(var.set_gyro_lpf_mode(config[CONF_GYROSCOPE_LPF_MODE]))
    cg.add(var.set_gyro_odr(config[CONF_GYROSCOPE_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYROSCOPE_RANGE]))

    for d in ["x", "y", "z"]:
        accel_key = f"acceleration_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f"set_accel_{d}_sensor")(sens))
        accel_key = f"gyroscope_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f"set_gyro_{d}_sensor")(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
