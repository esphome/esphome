import voluptuous as vol

from esphome.automation import ACTION_REGISTRY, maybe_simple_id
# from esphome.components.power_supply import PowerSupplyComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import power_supply
from esphome.const import CONF_ID, CONF_INVERTED, CONF_LEVEL, CONF_MAX_POWER, \
    CONF_MIN_POWER, CONF_POWER_SUPPLY
from esphome.core import CORE, coroutine

IS_PLATFORM_COMPONENT = True

BINARY_OUTPUT_SCHEMA = cv.Schema({
    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(power_supply.PowerSupply),
    vol.Optional(CONF_INVERTED): cv.boolean,
})

FLOAT_OUTPUT_SCHEMA = BINARY_OUTPUT_SCHEMA.extend({
    vol.Optional(CONF_MAX_POWER): cv.percentage,
    vol.Optional(CONF_MIN_POWER): cv.percentage,
})

output_ns = cg.esphome_ns.namespace('output')
BinaryOutput = output_ns.class_('BinaryOutput')
BinaryOutputPtr = BinaryOutput.operator('ptr')
FloatOutput = output_ns.class_('FloatOutput', BinaryOutput)
FloatOutputPtr = FloatOutput.operator('ptr')

# Actions
TurnOffAction = output_ns.class_('TurnOffAction', cg.Action)
TurnOnAction = output_ns.class_('TurnOnAction', cg.Action)
SetLevelAction = output_ns.class_('SetLevelAction', cg.Action)


@coroutine
def setup_output_platform_(obj, config):
    if CONF_INVERTED in config:
        cg.add(obj.set_inverted(config[CONF_INVERTED]))
    if CONF_POWER_SUPPLY in config:
        power_supply_ = yield cg.get_variable(config[CONF_POWER_SUPPLY])
        cg.add(obj.set_power_supply(power_supply_))
    if CONF_MAX_POWER in config:
        cg.add(obj.set_max_power(config[CONF_MAX_POWER]))
    if CONF_MIN_POWER in config:
        cg.add(obj.set_min_power(config[CONF_MIN_POWER]))


@coroutine
def register_output(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    yield setup_output_platform_(var, config)


BINARY_OUTPUT_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(BinaryOutput),
})


@ACTION_REGISTRY.register('output.turn_on', BINARY_OUTPUT_ACTION_SCHEMA)
def output_turn_on_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = TurnOnAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('output.turn_off', BINARY_OUTPUT_ACTION_SCHEMA)
def output_turn_off_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = TurnOffAction.template(template_arg)
    rhs = type.new(var)
    yield cg.Pvariable(action_id, rhs, type=type)


@ACTION_REGISTRY.register('output.set_level', cv.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(FloatOutput),
    vol.Required(CONF_LEVEL): cv.templatable(cv.percentage),
}))
def output_set_level_to_code(config, action_id, template_arg, args):
    var = yield cg.get_variable(config[CONF_ID])
    type = SetLevelAction.template(template_arg)
    rhs = type.new(var)
    action = cg.Pvariable(action_id, rhs, type=type)
    template_ = yield cg.templatable(config[CONF_LEVEL], args, float)
    cg.add(action.set_level(template_))
    yield action


def to_code(config):
    cg.add_global(output_ns.using)

