import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_ID,
    CONF_NITROGEN_DIOXIDE,
    CONF_TVOC,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ICON_AIR_FILTER,
    ICON_FLASK_ROUND_BOTTOM,
    ICON_GAS_CYLINDER,
    ICON_MOLECULE_CO,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@YorkshireIoT"]
DEPENDENCIES = ["i2c"]

grove_gas_mc_v2_ns = cg.esphome_ns.namespace("grove_gas_mc_v2")

GroveGasMultichannelV2Component = grove_gas_mc_v2_ns.class_(
    "GroveGasMultichannelV2Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GroveGasMultichannelV2Component),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_AIR_FILTER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CARBON_MONOXIDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_NITROGEN_DIOXIDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_GAS_CYLINDER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_NITROGEN_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ETHANOL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_FLASK_ROUND_BOTTOM,
                accuracy_decimals=0,
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

    for key in [CONF_TVOC, CONF_CARBON_MONOXIDE, CONF_NITROGEN_DIOXIDE, CONF_ETHANOL]:
        if sensor_config := config.get(key):
            sensor_ = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}_sensor")(sensor_))
