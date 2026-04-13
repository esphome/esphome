from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
    CONF_MODE,
    CONF_POWER_MODE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]

tas2780_ns = cg.esphome_ns.namespace("tas2780")
TAS2780 = tas2780_ns.class_("TAS2780", AudioDac, cg.Component, i2c.I2CDevice)

ResetAction = tas2780_ns.class_(
    "ResetAction", automation.Action, cg.Parented.template(TAS2780)
)

ActivateAction = tas2780_ns.class_(
    "ActivateAction", automation.Action, cg.Parented.template(TAS2780)
)

UpdateConfigAction = tas2780_ns.class_(
    "UpdateConfigAction", automation.Action, cg.Parented.template(TAS2780)
)

DeactivateAction = tas2780_ns.class_(
    "DeactivateAction", automation.Action, cg.Parented.template(TAS2780)
)

CONF_VOL_RANGE_MIN = "vol_range_min"
CONF_VOL_RANGE_MAX = "vol_range_max"
CONF_AMP_LEVEL = "amp_level"
CONF_PVDD_SENSOR = "pvdd_sensor"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TAS2780),
            cv.Optional(CONF_AMP_LEVEL, default=8): cv.int_range(min=0, max=20),
            cv.Optional(CONF_POWER_MODE, default=2): cv.int_range(min=0, max=3),
            cv.Optional(CONF_VOL_RANGE_MIN, default=0.3): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_VOL_RANGE_MAX, default=1.0): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=2),
            cv.Optional(CONF_PVDD_SENSOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x38))
)


TAS2780_BASE_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(TAS2780)})

TAS2780_ACTIVATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(TAS2780),
        cv.Optional(CONF_MODE, default=2): cv.templatable(cv.int_range(min=0, max=3)),
    }
)


@automation.register_action(
    "tas2780.deactivate", DeactivateAction, TAS2780_BASE_ACTION_SCHEMA, synchronous=True
)
async def tas2780_deactivate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "tas2780.reset", ResetAction, TAS2780_BASE_ACTION_SCHEMA, synchronous=True
)
async def tas2780_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "tas2780.activate", ActivateAction, TAS2780_ACTIVATE_ACTION_SCHEMA, synchronous=True
)
async def tas2780_activate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    mode = config.get(CONF_MODE)
    template = await cg.templatable(mode, args, cg.uint8)
    cg.add(var.set_mode(template))
    return var


TAS2780_UPDATE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(TAS2780),
        cv.Optional(CONF_VOL_RANGE_MIN): cv.templatable(
            cv.float_range(min=0.0, max=1.0)
        ),
        cv.Optional(CONF_VOL_RANGE_MAX): cv.templatable(
            cv.float_range(min=0.0, max=1.0)
        ),
        cv.Optional(CONF_AMP_LEVEL): cv.templatable(cv.int_range(min=0, max=20)),
        cv.Optional(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=2)),
    }
)


@automation.register_action(
    "tas2780.update_config",
    UpdateConfigAction,
    TAS2780_UPDATE_CONFIG_SCHEMA,
    synchronous=True,
)
async def tas2780_update_config_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if CONF_VOL_RANGE_MIN in config:
        template = await cg.templatable(config[CONF_VOL_RANGE_MIN], args, float)
        cg.add(var.set_vol_range_min(template))
    if CONF_VOL_RANGE_MAX in config:
        template = await cg.templatable(config[CONF_VOL_RANGE_MAX], args, float)
        cg.add(var.set_vol_range_max(template))
    if CONF_AMP_LEVEL in config:
        template = await cg.templatable(config[CONF_AMP_LEVEL], args, cg.uint8)
        cg.add(var.set_amp_level(template))
    if CONF_CHANNEL in config:
        template = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
        cg.add(var.set_channel(template))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_amp_level(config[CONF_AMP_LEVEL]))
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_vol_range_min(config[CONF_VOL_RANGE_MIN]))
    cg.add(var.set_vol_range_max(config[CONF_VOL_RANGE_MAX]))
    cg.add(var.set_selected_channel(config[CONF_CHANNEL]))

    if CONF_PVDD_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_PVDD_SENSOR])
        cg.add(var.set_pvdd_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
