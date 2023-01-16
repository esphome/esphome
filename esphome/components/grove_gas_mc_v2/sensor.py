import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_ETHANOL,
    CONF_CARBON_MONOXIDE,
    CONF_NO2,
    CONF_TVOC,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_ETHANOL,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    STATE_CLASS_MEASUREMENT,
    ICON_AIR_FILTER,
    ICON_MOLECULE_CO,
    ICON_FLASK_ROUND_BOTTOM,
    ICON_GAS_CYLINDER,
    UNIT_EMPTY,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@YorkshireIoT"]

grove_gas_mc_v2_ns = cg.esphome_ns.namespace("grove_gas_mc_v2")

GroveGasMultichannelV2Component = grove_gas_mc_v2_ns.class_(
    "GroveGasMultichannelV2Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GroveGasMultichannelV2Component),
            cv.Required(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_AIR_FILTER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_CARBON_MONOXIDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_MOLECULE_CO,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_NO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_GAS_CYLINDER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_NITROGEN_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_ETHANOL): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_FLASK_ROUND_BOTTOM,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ETHANOL,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x08))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sens = await sensor.new_sensor(config[CONF_TVOC])
    cg.add(var.set_tvoc(sens))

    sens = await sensor.new_sensor(config[CONF_CARBON_MONOXIDE])
    cg.add(var.set_carbon_monoxide(sens))

    sens = await sensor.new_sensor(config[CONF_NO2])
    cg.add(var.set_no2(sens))

    sens = await sensor.new_sensor(config[CONF_ETHANOL])
    cg.add(var.set_ethanol(sens))
