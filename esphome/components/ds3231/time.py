from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TEMPERATURE

CODEOWNERS = ["@badbadc0ffee"]
DEPENDENCIES = ["i2c"]

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_("DS3231Component", time.RealTimeClock, i2c.I2CDevice)
WriteAction = ds3231_ns.class_("WriteAction", automation.Action)
ReadAction = ds3231_ns.class_("ReadAction", automation.Action)

CONF_DS3231 = "ds3231"

CONFIG_SCHEMA = (
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DS3231Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x68))
    .extend(cv.COMPONENT_SCHEMA)
)


@automation.register_action(
    "ds3231.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
        }
    ),
)
async def ds3231_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3231.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
        }
    ),
)
async def ds3231_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
    
    # Register temperature sensor if configured
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
