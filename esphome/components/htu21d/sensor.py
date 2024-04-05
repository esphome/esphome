import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome import automation
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MODEL,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    CONF_HEATER,
    UNIT_EMPTY,
    CONF_LEVEL,
    CONF_STATUS,
)

DEPENDENCIES = ["i2c"]

htu21d_ns = cg.esphome_ns.namespace("htu21d")
HTU21DComponent = htu21d_ns.class_(
    "HTU21DComponent", cg.PollingComponent, i2c.I2CDevice
)
SetHeaterLevelAction = htu21d_ns.class_("SetHeaterLevelAction", automation.Action)
SetHeaterAction = htu21d_ns.class_("SetHeaterAction", automation.Action)
HTU21DSensorModels = htu21d_ns.enum("HTU21DSensorModels")

MODELS = {
    "HTU21D": HTU21DSensorModels.HTU21D_SENSOR_MODEL_HTU21D,
    "SI7021": HTU21DSensorModels.HTU21D_SENSOR_MODEL_SI7021,
    "SHT21": HTU21DSensorModels.HTU21D_SENSOR_MODEL_SHT21,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HTU21DComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HEATER): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MODEL, default="HTU21D"): cv.enum(MODELS, upper=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))

    if CONF_HEATER in config:
        sens = await sensor.new_sensor(config[CONF_HEATER])
        cg.add(var.set_heater(sens))

    cg.add(var.set_sensor_model(config[CONF_MODEL]))


@automation.register_action(
    "htu21d.set_heater_level",
    SetHeaterLevelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HTU21DComponent),
            cv.Required(CONF_LEVEL): cv.templatable(cv.int_),
        },
        key=CONF_LEVEL,
    ),
)
async def set_heater_level_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    level_ = await cg.templatable(config[CONF_LEVEL], args, int)
    cg.add(var.set_level(level_))
    return var


@automation.register_action(
    "htu21d.set_heater",
    SetHeaterAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(HTU21DComponent),
            cv.Required(CONF_STATUS): cv.templatable(cv.boolean),
        },
        key=CONF_STATUS,
    ),
)
async def set_heater_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    status_ = await cg.templatable(config[CONF_LEVEL], args, bool)
    cg.add(var.set_status(status_))
    return var
