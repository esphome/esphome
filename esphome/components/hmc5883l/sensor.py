import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_OVERSAMPLING,
    CONF_RANGE,
    ICON_MAGNET,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROTESLA,
    UNIT_DEGREES,
    ICON_SCREEN_ROTATION,
    CONF_UPDATE_INTERVAL,
)

DEPENDENCIES = ["i2c"]

hmc5883l_ns = cg.esphome_ns.namespace("hmc5883l")

CONF_FIELD_STRENGTH_X = "field_strength_x"
CONF_FIELD_STRENGTH_Y = "field_strength_y"
CONF_FIELD_STRENGTH_Z = "field_strength_z"
CONF_HEADING = "heading"

HMC5883LComponent = hmc5883l_ns.class_(
    "HMC5883LComponent", cg.PollingComponent, i2c.I2CDevice
)

HMC5883LOversampling = hmc5883l_ns.enum("HMC5883LOversampling")
HMC5883LOversamplings = {
    1: HMC5883LOversampling.HMC5883L_OVERSAMPLING_1,
    2: HMC5883LOversampling.HMC5883L_OVERSAMPLING_2,
    4: HMC5883LOversampling.HMC5883L_OVERSAMPLING_4,
    8: HMC5883LOversampling.HMC5883L_OVERSAMPLING_8,
}

HMC5883LDatarate = hmc5883l_ns.enum("HMC5883LDatarate")
HMC5883LDatarates = {
    0.75: HMC5883LDatarate.HMC5883L_DATARATE_0_75_HZ,
    1.5: HMC5883LDatarate.HMC5883L_DATARATE_1_5_HZ,
    3.0: HMC5883LDatarate.HMC5883L_DATARATE_3_0_HZ,
    7.5: HMC5883LDatarate.HMC5883L_DATARATE_7_5_HZ,
    15: HMC5883LDatarate.HMC5883L_DATARATE_15_0_HZ,
    30: HMC5883LDatarate.HMC5883L_DATARATE_30_0_HZ,
    75: HMC5883LDatarate.HMC5883L_DATARATE_75_0_HZ,
}

HMC5883LRange = hmc5883l_ns.enum("HMC5883LRange")
HMC5883L_RANGES = {
    88: HMC5883LRange.HMC5883L_RANGE_88_UT,
    130: HMC5883LRange.HMC5883L_RANGE_130_UT,
    190: HMC5883LRange.HMC5883L_RANGE_190_UT,
    250: HMC5883LRange.HMC5883L_RANGE_250_UT,
    400: HMC5883LRange.HMC5883L_RANGE_400_UT,
    470: HMC5883LRange.HMC5883L_RANGE_470_UT,
    560: HMC5883LRange.HMC5883L_RANGE_560_UT,
    810: HMC5883LRange.HMC5883L_RANGE_810_UT,
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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HMC5883LComponent),
            cv.Optional(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_OVERSAMPLING, default="1x"): validate_enum(
                HMC5883LOversamplings, units="x"
            ),
            cv.Optional(CONF_RANGE, default="130µT"): validate_enum(
                HMC5883L_RANGES, units=["uT", "µT"]
            ),
            cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
            cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
            cv.Optional(CONF_HEADING): heading_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x1E))
)


def auto_data_rate(config):
    interval_msec = config[CONF_UPDATE_INTERVAL].total_milliseconds
    interval_hz = 1000.0 / interval_msec
    for datarate in sorted(HMC5883LDatarates.keys()):
        if float(datarate) >= interval_hz:
            return HMC5883LDatarates[datarate]
    return HMC5883LDatarates[75]


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
