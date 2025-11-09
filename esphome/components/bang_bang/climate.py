from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_AWAY_CONFIG,
    CONF_COOL_ACTION,
    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH,
    CONF_DEFAULT_TARGET_TEMPERATURE_LOW,
    CONF_HEAT_ACTION,
    CONF_HUMIDITY_SENSOR,
    CONF_IDLE_ACTION,
    CONF_SENSOR,
)

bang_bang_ns = cg.esphome_ns.namespace("bang_bang")
BangBangClimate = bang_bang_ns.class_("BangBangClimate", climate.Climate, cg.Component)
BangBangClimateTargetTempConfig = bang_bang_ns.struct("BangBangClimateTargetTempConfig")

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(BangBangClimate)
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
            cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
            cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
            cv.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_COOL_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_HEAT_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_AWAY_CONFIG): cv.Schema(
                {
                    cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE_LOW): cv.temperature,
                    cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE_HIGH): cv.temperature,
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_COOL_ACTION, CONF_HEAT_ACTION),
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    if CONF_HUMIDITY_SENSOR in config:
        sens = await cg.get_variable(config[CONF_HUMIDITY_SENSOR])
        cg.add(var.set_humidity_sensor(sens))

    normal_config = BangBangClimateTargetTempConfig(
        config[CONF_DEFAULT_TARGET_TEMPERATURE_LOW],
        config[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH],
    )
    cg.add(var.set_normal_config(normal_config))

    await automation.build_automation(
        var.get_idle_trigger(), [], config[CONF_IDLE_ACTION]
    )

    if cool_action_config := config.get(CONF_COOL_ACTION):
        await automation.build_automation(
            var.get_cool_trigger(), [], cool_action_config
        )
        cg.add(var.set_supports_cool(True))
    if heat_action_config := config.get(CONF_HEAT_ACTION):
        await automation.build_automation(
            var.get_heat_trigger(), [], heat_action_config
        )
        cg.add(var.set_supports_heat(True))

    if away := config.get(CONF_AWAY_CONFIG):
        away_config = BangBangClimateTargetTempConfig(
            away[CONF_DEFAULT_TARGET_TEMPERATURE_LOW],
            away[CONF_DEFAULT_TARGET_TEMPERATURE_HIGH],
        )
        cg.add(var.set_away_config(away_config))
