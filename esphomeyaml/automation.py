import voluptuous as vol

from esphomeyaml import core
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ABOVE, CONF_ACTION_ID, CONF_AND, CONF_AUTOMATION_ID, \
    CONF_BELOW, CONF_CONDITION, CONF_CONDITION_ID, CONF_DELAY, \
    CONF_ELSE, CONF_ID, CONF_IF, CONF_LAMBDA, \
    CONF_OR, CONF_RANGE, CONF_THEN, CONF_TRIGGER_ID
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, TemplateArguments, add, add_job, \
    esphomelib_ns, float_, process_lambda, templatable, uint32, get_variable
from esphomeyaml.util import ServiceRegistry


def maybe_simple_id(*validators):
    validator = vol.All(*validators)

    def validate(value):
        if isinstance(value, dict):
            return validator(value)
        return validator({CONF_ID: value})

    return validate


def validate_recursive_condition(value):
    return CONDITIONS_SCHEMA(value)


def validate_recursive_action(value):
    value = cv.ensure_list(value)
    for i, item in enumerate(value):
        if not isinstance(item, dict):
            raise vol.Invalid(u"Action must consist of key-value mapping! Got {}".format(item))
        key = next((x for x in item if x != CONF_ACTION_ID), None)
        if key is None:
            raise vol.Invalid(u"Key missing from action! Got {}".format(item))
        if key not in ACTION_REGISTRY:
            raise vol.Invalid(u"Unable to find action with the name '{}', is the component loaded?"
                              u"".format(key))
        item.setdefault(CONF_ACTION_ID, None)
        key2 = next((x for x in item if x != CONF_ACTION_ID and x != key), None)
        if key2 is not None:
            raise vol.Invalid(u"Cannot have two actions in one item. Key {} overrides {}!"
                              u"".format(key, key2))
        validator = ACTION_REGISTRY[key][0]
        value[i] = {
            CONF_ACTION_ID: cv.declare_variable_id(None)(item[CONF_ACTION_ID]),
            key: validator(item[key])
        }
    return value


ACTION_REGISTRY = ServiceRegistry()

# pylint: disable=invalid-name
DelayAction = esphomelib_ns.DelayAction
LambdaAction = esphomelib_ns.LambdaAction
IfAction = esphomelib_ns.IfAction
UpdateComponentAction = esphomelib_ns.UpdateComponentAction
Automation = esphomelib_ns.Automation

CONDITIONS_SCHEMA = vol.All(cv.ensure_list, [cv.templatable({
    cv.GenerateID(CONF_CONDITION_ID): cv.declare_variable_id(None),
    vol.Optional(CONF_AND): validate_recursive_condition,
    vol.Optional(CONF_OR): validate_recursive_condition,
    vol.Optional(CONF_RANGE): vol.All(vol.Schema({
        vol.Optional(CONF_ABOVE): vol.Coerce(float),
        vol.Optional(CONF_BELOW): vol.Coerce(float),
    }), cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW)),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
})])

# pylint: disable=invalid-name
AndCondition = esphomelib_ns.AndCondition
OrCondition = esphomelib_ns.OrCondition
RangeCondition = esphomelib_ns.RangeCondition
LambdaCondition = esphomelib_ns.LambdaCondition


def validate_automation(extra_schema=None, extra_validators=None, single=False):
    schema = AUTOMATION_SCHEMA.extend(extra_schema or {})

    def validator_(value):
        if isinstance(value, list):
            try:
                # First try as a sequence of actions
                return [schema({CONF_THEN: value})]
            except vol.Invalid as err:
                # Next try as a sequence of automations
                try:
                    return vol.Schema([schema])(value)
                except vol.Invalid as err2:
                    raise vol.MultipleInvalid([err, err2])
        elif isinstance(value, dict):
            if CONF_THEN in value:
                return [schema(value)]
            return [schema({CONF_THEN: value})]
        # This should only happen with invalid configs, but let's have a nice error message.
        return [schema(value)]

    def validator(value):
        value = validator_(value)
        if extra_validators is not None:
            value = vol.Schema([extra_validators])(value)
        if single:
            if len(value) != 1:
                raise vol.Invalid("Cannot have more than 1 automation for templates")
            return value[0]
        return value

    return validator


AUTOMATION_SCHEMA = vol.Schema({
    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(None),
    cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_variable_id(None),
    vol.Optional(CONF_IF): CONDITIONS_SCHEMA,
    vol.Required(CONF_THEN): validate_recursive_action,
})


def build_condition(config, arg_type):
    template_arg = TemplateArguments(arg_type)
    if isinstance(config, core.Lambda):
        lambda_ = None
        for lambda_ in process_lambda(config, [(arg_type, 'x')]):
            yield
        yield LambdaCondition.new(template_arg, lambda_)
    elif CONF_AND in config:
        yield AndCondition.new(template_arg, build_conditions(config[CONF_AND], template_arg))
    elif CONF_OR in config:
        yield OrCondition.new(template_arg, build_conditions(config[CONF_OR], template_arg))
    elif CONF_LAMBDA in config:
        lambda_ = None
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(arg_type, 'x')]):
            yield
        yield LambdaCondition.new(template_arg, lambda_)
    elif CONF_RANGE in config:
        conf = config[CONF_RANGE]
        rhs = RangeCondition.new(template_arg)
        type = RangeCondition.template(template_arg)
        condition = Pvariable(config[CONF_CONDITION_ID], rhs, type=type)
        if CONF_ABOVE in conf:
            template_ = None
            for template_ in templatable(conf[CONF_ABOVE], arg_type, float_):
                yield
            condition.set_min(template_)
        if CONF_BELOW in conf:
            template_ = None
            for template_ in templatable(conf[CONF_BELOW], arg_type, float_):
                yield
            condition.set_max(template_)
        yield condition
    else:
        raise ESPHomeYAMLError(u"Unsupported condition {}".format(config))


def build_conditions(config, arg_type):
    conditions = []
    for conf in config:
        condition = None
        for condition in build_condition(conf, arg_type):
            yield None
        conditions.append(condition)
    yield ArrayInitializer(*conditions)


DELAY_ACTION_SCHEMA = cv.templatable(cv.positive_time_period_milliseconds)


@ACTION_REGISTRY.register(CONF_DELAY, DELAY_ACTION_SCHEMA)
def delay_action_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    rhs = App.register_component(DelayAction.new(template_arg))
    type = DelayAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config, arg_type, uint32):
        yield
    add(action.set_delay(template_))
    yield action


IF_ACTION_SCHEMA = vol.All({
    vol.Required(CONF_CONDITION): validate_recursive_condition,
    vol.Optional(CONF_THEN): validate_recursive_action,
    vol.Optional(CONF_ELSE): validate_recursive_action,
}, cv.has_at_least_one_key(CONF_THEN, CONF_ELSE))


@ACTION_REGISTRY.register(CONF_IF, IF_ACTION_SCHEMA)
def if_action_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for conditions in build_conditions(config[CONF_CONDITION], arg_type):
        yield None
    rhs = IfAction.new(template_arg, conditions)
    type = IfAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_THEN in config:
        for actions in build_actions(config[CONF_THEN], arg_type):
            yield None
        add(action.add_then(actions))
    if CONF_ELSE in config:
        for actions in build_actions(config[CONF_ELSE], arg_type):
            yield None
        add(action.add_else(actions))
    yield action


LAMBDA_ACTION_SCHEMA = cv.lambda_


@ACTION_REGISTRY.register(CONF_LAMBDA, LAMBDA_ACTION_SCHEMA)
def lambda_action_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for lambda_ in process_lambda(config, [(arg_type, 'x')]):
        yield None
    rhs = LambdaAction.new(template_arg, lambda_)
    type = LambdaAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


CONF_COMPONENT_UPDATE = 'component.update'
COMPONENT_UPDATE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(None),
})


@ACTION_REGISTRY.register(CONF_COMPONENT_UPDATE, COMPONENT_UPDATE_ACTION_SCHEMA)
def component_update_action_to_code(config, action_id, arg_type):
    template_arg = TemplateArguments(arg_type)
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = UpdateComponentAction.new(var)
    type = UpdateComponentAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


def build_action(full_config, arg_type):
    action_id = full_config[CONF_ACTION_ID]
    key, config = next((k, v) for k, v in full_config.items() if k in ACTION_REGISTRY)

    builder = ACTION_REGISTRY[key][1]
    for result in builder(config, action_id, arg_type):
        yield None
    yield result


def build_actions(config, arg_type):
    actions = []
    for conf in config:
        action = None
        for action in build_action(conf, arg_type):
            yield None
        actions.append(action)
    yield ArrayInitializer(*actions, multiline=False)


def build_automation_(trigger, arg_type, config):
    rhs = App.make_automation(TemplateArguments(arg_type), trigger)
    type = Automation.template(arg_type)
    obj = Pvariable(config[CONF_AUTOMATION_ID], rhs, type=type)
    if CONF_IF in config:
        conditions = None
        for conditions in build_conditions(config[CONF_IF], arg_type):
            yield None
        add(obj.add_conditions(conditions))
    actions = None
    for actions in build_actions(config[CONF_THEN], arg_type):
        yield None
    add(obj.add_actions(actions))
    yield obj


def build_automation(trigger, arg_type, config):
    add_job(build_automation_, trigger, arg_type, config)
