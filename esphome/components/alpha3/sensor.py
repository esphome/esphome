import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client
from esphome.const import (
    CONF_ID,
    CONF_CURRENT,
    CONF_FLOW,
    CONF_HEAD,
    CONF_POWER,
    CONF_SPEED,
    CONF_VOLTAGE,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_METER,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

alpha3_ns = cg.esphome_ns.namespace("alpha3")
Alpha3 = alpha3_ns.class_("Alpha3", ble_client.BLEClientNode, cg.PollingComponent)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Alpha3),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_HEAD): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=2,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("15s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_FLOW])
        cg.add(var.set_flow_sensor(sens))

    if CONF_HEAD in config:
        sens = await sensor.new_sensor(config[CONF_HEAD])
        cg.add(var.set_head_sensor(sens))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))

    if CONF_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))

    if CONF_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_SPEED])
        cg.add(var.set_speed_sensor(sens))

    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_voltage_sensor(sens))
