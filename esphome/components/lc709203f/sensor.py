import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    UNIT_VOLT,
    ICON_EMPTY,
    UNIT_PERCENT,
    UNIT_EMPTY,
)

DEPENDENCIES = ["i2c"]

# locally defined constant

lc709203f_ns = cg.esphome_ns.namespace("lc709203f")

LC709203FComponent = lc709203f_ns.class_(
    "LC709203FComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LC709203FComponent),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT, icon=ICON_EMPTY, accuracy_decimals=2
            ).extend({}),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT, icon=ICON_EMPTY, accuracy_decimals=1
            ).extend({}),
            cv.Optional("cell_charge"): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT, icon=ICON_EMPTY, accuracy_decimals=0
            ).extend({}),
            # cv.Optional('icversion'): cv.uint16_t,
            cv.Optional("icversion"): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY, icon=ICON_EMPTY, accuracy_decimals=0
            ).extend({}),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x77))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_BATTERY_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_cellRemPercent_sensor(sens))

    if CONF_BATTERY_VOLTAGE in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(var.set_cellVoltage_sensor(sens))

    if "cell_charge" in config:
        sens = yield sensor.new_sensor(config["cell_charge"])
        cg.add(var.set_cellCharge_sensor(sens))

    if "icversion" in config:
        sens = yield sensor.new_sensor(config["icversion"])
        cg.add(var.set_icversion_sensor(sens))


# / read-only values
# / implement sensors for cellVoltage_mV, cellRemainingPercent10, cellStateOfCharge, getICversion

# / write-only values
# / implement config for setPowerMode, setCellCapacity, setCellProfile, setAlarmRSOC, setAlarmVoltage

# / uninstalled ( exsbc source )
# / getThermistorBeta, setThermistorB, setTemperatureMode, getCellTemperature, initRSOC
