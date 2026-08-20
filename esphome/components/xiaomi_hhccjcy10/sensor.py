import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_CONDUCTIVITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FLOWER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_LUX,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    UNIT_PERCENT,
)

AUTO_LOAD = ["ble_device_base"]

xiaomi_hhccjcy10_ns = cg.esphome_ns.namespace("xiaomi_hhccjcy10")
XiaomiHHCCJCY10 = xiaomi_hhccjcy10_ns.class_(
    "XiaomiHHCCJCY10", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_hhccjcy10"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiHHCCJCY10),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSIEMENS_PER_CENTIMETER,
                icon=ICON_FLOWER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature(sens))
    if moisture_config := config.get(CONF_MOISTURE):
        sens = await sensor.new_sensor(moisture_config)
        cg.add(var.set_moisture(sens))
    if illuminance_config := config.get(CONF_ILLUMINANCE):
        sens = await sensor.new_sensor(illuminance_config)
        cg.add(var.set_illuminance(sens))
    if conductivity_config := config.get(CONF_CONDUCTIVITY):
        sens = await sensor.new_sensor(conductivity_config)
        cg.add(var.set_conductivity(sens))
    if battery_level_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(battery_level_config)
        cg.add(var.set_battery_level(sens))
