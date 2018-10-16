import voluptuous as vol

from esphomeyaml.automation import maybe_simple_id, ACTION_REGISTRY
import esphomeyaml.config_validation as cv
from esphomeyaml.components.power_supply import PowerSupplyComponent
from esphomeyaml.const import CONF_INVERTED, CONF_MAX_POWER, CONF_POWER_SUPPLY, CONF_ID, CONF_LEVEL
from esphomeyaml.helpers import add, esphomelib_ns, get_variable, TemplateArguments, Pvariable, \
    templatable, bool_

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

BINARY_OUTPUT_SCHEMA = vol.Schema({
    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(PowerSupplyComponent),
    vol.Optional(CONF_INVERTED): cv.boolean,
})

BINARY_OUTPUT_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(BINARY_OUTPUT_SCHEMA.schema)

FLOAT_OUTPUT_SCHEMA = BINARY_OUTPUT_SCHEMA.extend({
    vol.Optional(CONF_MAX_POWER): cv.percentage,
})

FLOAT_OUTPUT_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(FLOAT_OUTPUT_SCHEMA.schema)

output_ns = esphomelib_ns.namespace('output')
TurnOffAction = output_ns.TurnOffAction
TurnOnAction = output_ns.TurnOnAction
SetLevelAction = output_ns.SetLevelAction


def setup_output_platform_(obj, config, skip_power_supply=False):
    if CONF_INVERTED in config:
        add(obj.set_inverted(config[CONF_INVERTED]))
    if not skip_power_supply and CONF_POWER_SUPPLY in config:
        power_supply = None
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
        add(obj.set_power_supply(power_supply))
    if CONF_MAX_POWER in config:
        add(obj.set_max_power(config[CONF_MAX_POWER]))


def setup_output_platform(obj, config, skip_power_supply=False):
    for _ in setup_output_platform_(obj, config, skip_power_supply):
        yield


BUILD_FLAGS = '-DUSE_OUTPUT'


CONF_OUTPUT_TURN_ON = 'output.turn_on'
OUTPUT_TURN_OFF_ACTION = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_OUTPUT_TURN_ON, OUTPUT_TURN_OFF_ACTION)
def output_turn_on_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_off_action(template_arg)
    type = TurnOffAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_OUTPUT_TURN_OFF = 'output.turn_off'
OUTPUT_TURN_ON_ACTION = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None)
})


@ACTION_REGISTRY.register(CONF_OUTPUT_TURN_OFF, OUTPUT_TURN_ON_ACTION)
def output_turn_off_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_turn_on_action(template_arg)
    type = TurnOnAction.template(arg_type)
    yield Pvariable(action_id, rhs, type=type)


CONF_OUTPUT_SET_LEVEL = 'output.set_level'
OUTPUT_SET_LEVEL_ACTION = vol.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(None),
    vol.Required(CONF_LEVEL): cv.percentage,
})


@ACTION_REGISTRY.register(CONF_OUTPUT_SET_LEVEL, OUTPUT_SET_LEVEL_ACTION)
def output_set_level_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_set_level_action(template_arg)
    type = SetLevelAction.template(arg_type)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_LEVEL], arg_type, bool_):
        yield None
    add(action.set_level(template_))
    yield action
