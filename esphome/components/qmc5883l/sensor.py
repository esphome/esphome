import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_FIELD_STRENGTH_X,
    CONF_FIELD_STRENGTH_Y,
    CONF_FIELD_STRENGTH_Z,
    CONF_HEADING,
    CONF_TEMPERATURE,
    CONF_ID,
    CONF_OVERSAMPLING,
    CONF_RANGE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MAGNET,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROTESLA,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    ICON_SCREEN_ROTATION,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["i2c"]

qmc5883l_ns = cg.esphome_ns.namespace("qmc5883l")

QMC5883LComponent = qmc5883l_ns.class_(
    "QMC5883LComponent", cg.PollingComponent, i2c.I2CDevice
)

QMC5883LDatarate = qmc5883l_ns.enum("QMC5883LDatarate")
QMC5883LDatarates = {
    10: QMC5883LDatarate.QMC5883L_DATARATE_10_HZ,
    50: QMC5883LDatarate.QMC5883L_DATARATE_50_HZ,
    100: QMC5883LDatarate.QMC5883L_DATARATE_100_HZ,
    200: QMC5883LDatarate.QMC5883L_DATARATE_200_HZ,
}

QMC5883LRange = qmc5883l_ns.enum("QMC5883LRange")
QMC5883L_RANGES = {
    200: QMC5883LRange.QMC5883L_RANGE_200_UT,
    800: QMC5883LRange.QMC5883L_RANGE_800_UT,
}

QMC5883LOversampling = qmc5883l_ns.enum("QMC5883LOversampling")
QMC5883LOversamplings = {
    512: QMC5883LOversampling.QMC5883L_SAMPLING_512,
    256: QMC5883LOversampling.QMC5883L_SAMPLING_256,
    128: QMC5883LOversampling.QMC5883L_SAMPLING_128,
    64: QMC5883LOversampling.QMC5883L_SAMPLING_64,
}


def validate_enum(enum_values, units=None, int=True):
    _units = []
    if units is not None:
        _units = units if isinstance(units, list) else [units]
        _units = [str(x) for x in _units]
    enum_bound = cv.enum(enum_values, int=int)

    def validate_enum_bound(value):
        value = cv.string(value)
        for unit in _units:
            if value.endswith(unit):
                value = value[: -len(unit)]
                break
        return enum_bound(value)

    return validate_enum_bound


field_strength_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_MICROTESLA,
    icon=ICON_MAGNET,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
heading_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon=ICON_SCREEN_ROTATION,
    accuracy_decimals=1,
)
temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QMC5883LComponent),
            cv.Optional(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_RANGE, default="200µT"): validate_enum(
                QMC5883L_RANGES, units=["uT", "µT"]
            ),
            cv.Optional(CONF_OVERSAMPLING, default="512x"): validate_enum(
                QMC5883LOversamplings, units="x"
            ),
            cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
            cv.Optional(CONF_HEADING): heading_schema,
            cv.Optional(CONF_TEMPERATURE): temperature_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x0D))
)


def auto_data_rate(config):
    interval_sec = config[CONF_UPDATE_INTERVAL].total_milliseconds / 1000
    interval_hz = 1.0 / interval_sec
    for datarate in sorted(QMC5883LDatarates.keys()):
        if float(datarate) >= interval_hz:
            return QMC5883LDatarates[datarate]
    return QMC5883LDatarates[200]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_datarate(auto_data_rate(config)))
    cg.add(var.set_range(config[CONF_RANGE]))
    if CONF_FIELD_STRENGTH_X in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_X])
        cg.add(var.set_x_sensor(sens))
    if CONF_FIELD_STRENGTH_Y in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_Y])
        cg.add(var.set_y_sensor(sens))
    if CONF_FIELD_STRENGTH_Z in config:
        sens = await sensor.new_sensor(config[CONF_FIELD_STRENGTH_Z])
        cg.add(var.set_z_sensor(sens))
    if CONF_HEADING in config:
        sens = await sensor.new_sensor(config[CONF_HEADING])
        cg.add(var.set_heading_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
