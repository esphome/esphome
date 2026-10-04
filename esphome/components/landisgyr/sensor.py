import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENERGY,
    CONF_ID,
    CONF_VOLUME,
    ICON_HEATING_COIL,
    ICON_WATER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CUBIC_METER,
    UNIT_KILOWATT_HOURS,
)

DEPENDENCIES = ["uart"]

landis_sensor_ns = cg.esphome_ns.namespace("landisgyr")

LandisSensor = landis_sensor_ns.class_(
    "LandisSensor ", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LandisSensor),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_HEATING_COIL,
            ),
            cv.Optional(CONF_VOLUME): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_WATER,
            ),
        }
    )
    .extend(cv.polling_component_schema("15s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_ENERGY])
        cg.add(var.set_kwh_sensor(sens))

    if CONF_VOLUME in config:
        sens = await sensor.new_sensor(config[CONF_VOLUME])
        cg.add(var.set_volume_sensor(sens))
