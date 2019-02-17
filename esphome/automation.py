import copy

import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import CONF_ABOVE, CONF_ACTION_ID, CONF_AND, CONF_AUTOMATION_ID, \
    CONF_BELOW, CONF_CONDITION, CONF_CONDITION_ID, CONF_DELAY, CONF_ELSE, CONF_ID, CONF_IF, \
    CONF_LAMBDA, CONF_OR, CONF_RANGE, CONF_THEN, CONF_TRIGGER_ID, CONF_WHILE, CONF_WAIT_UNTIL
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, TemplateArguments, add, get_variable, \
    process_lambda, templatable
from esphome.cpp_types import Action, App, Component, PollingComponent, Trigger, bool_, \
    esphome_ns, float_, uint32, void
from esphome.util import ServiceRegistry


def maybe_simple_id(*validators):
    validator = vol.All(*validators)

    def validate(value):
        if isinstance(value, dict):
            return validator(value)
        return validator({CONF_ID: value})

    return validate


def validate_recursive_condition(value):
    is_list = isinstance(value, list)
    value = cv.ensure_list()(value)[:]
    for i, item in enumerate(value):
        path = [i] if is_list else []
        item = copy.deepcopy(item)
        if not isinstance(item, dict):
            raise vol.Invalid(u"Condition must consist of key-value mapping! Got {}".format(item),
                              path)
        key = next((x for x in item if x != CONF_CONDITION_ID), None)
        if key is None:
            raise vol.Invalid(u"Key missing from action! Got {}".format(item), path)
        if key not in CONDITION_REGISTRY:
            raise vol.Invalid(u"Unable to find condition with the name '{}', is the "
                              u"component loaded?".format(key), path + [key])
        item.setdefault(CONF_CONDITION_ID, None)
        key2 = next((x for x in item if x not in (CONF_CONDITION_ID, key)), None)
        if key2 is not None:
            raise vol.Invalid(u"Cannot have two conditions in one item. Key '{}' overrides '{}'! "
                              u"Did you forget to indent the block inside the condition?"
                              u"".format(key, key2), path)
        validator = CONDITION_REGISTRY[key][0]
        try:
            condition = validator(item[key])
        except vol.Invalid as err:
            err.prepend(path)
            raise err
        value[i] = {
            CONF_CONDITION_ID: cv.declare_variable_id(Condition)(item[CONF_CONDITION_ID]),
            key: condition,
        }
    return value


def validate_recursive_action(value):
    is_list = isinstance(value, list)
    if not is_list:
        value = [value]
    for i, item in enumerate(value):
        path = [i] if is_list else []
        item = copy.deepcopy(item)
        if not isinstance(item, dict):
            raise vol.Invalid(u"Action must consist of key-value mapping! Got {}".format(item),
                              path)
        key = next((x for x in item if x != CONF_ACTION_ID), None)
        if key is None:
            raise vol.Invalid(u"Key missing from action! Got {}".format(item), path)
        if key not in ACTION_REGISTRY:
            raise vol.Invalid(u"Unable to find action with the name '{}', is the component loaded?"
                              u"".format(key), path + [key])
        item.setdefault(CONF_ACTION_ID, None)
        key2 = next((x for x in item if x not in (CONF_ACTION_ID, key)), None)
        if key2 is not None:
            raise vol.Invalid(u"Cannot have two actions in one item. Key '{}' overrides '{}'! "
                              u"Did you forget to indent the block inside the action?"
                              u"".format(key, key2), path)
        validator = ACTION_REGISTRY[key][0]
        try:
            action = validator(item[key])
        except vol.Invalid as err:
            err.prepend(path)
            raise err
        value[i] = {
            CONF_ACTION_ID: cv.declare_variable_id(Action)(item[CONF_ACTION_ID]),
            key: action,
        }
    return value


ACTION_REGISTRY = ServiceRegistry()
CONDITION_REGISTRY = ServiceRegistry()

# pylint: disable=invalid-name
DelayAction = esphome_ns.class_('DelayAction', Action, Component)
LambdaAction = esphome_ns.class_('LambdaAction', Action)
IfAction = esphome_ns.class_('IfAction', Action)
WhileAction = esphome_ns.class_('WhileAction', Action)
WaitUntilAction = esphome_ns.class_('WaitUntilAction', Action, Component)
UpdateComponentAction = esphome_ns.class_('UpdateComponentAction', Action)
Automation = esphome_ns.class_('Automation')

Condition = esphome_ns.class_('Condition')
AndCondition = esphome_ns.class_('AndCondition', Condition)
OrCondition = esphome_ns.class_('OrCondition', Condition)
RangeCondition = esphome_ns.class_('RangeCondition', Condition)
LambdaCondition = esphome_ns.class_('LambdaCondition', Condition)


def validate_automation(extra_schema=None, extra_validators=None, single=False):
    if extra_schema is None:
        extra_schema = {}
    if isinstance(extra_schema, vol.Schema):
        extra_schema = extra_schema.schema
    schema = AUTOMATION_SCHEMA.extend(extra_schema)

    def validator_(value):
        if isinstance(value, list):
            try:
                # First try as a sequence of actions
                return [schema({CONF_THEN: value})]
            except vol.Invalid as err:
                if err.path and err.path[0] == CONF_THEN:
                    err.path.pop(0)

                # Next try as a sequence of automations
                try:
                    return vol.Schema([schema])(value)
                except vol.Invalid as err2:
                    if 'Unable to find action' in str(err):
                        raise err2
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
    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(Trigger),
    cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_variable_id(Automation),
    vol.Optional(CONF_IF): validate_recursive_condition,
    vol.Required(CONF_THEN): validate_recursive_action,
})

AND_CONDITION_SCHEMA = validate_recursive_condition


@CONDITION_REGISTRY.register(CONF_AND, AND_CONDITION_SCHEMA)
def and_condition_to_code(config, condition_id, arg_type, template_arg):
    for conditions in build_conditions(config, arg_type):
        yield
    rhs = AndCondition.new(template_arg, conditions)
    type = AndCondition.template(template_arg)
    yield Pvariable(condition_id, rhs, type=type)


OR_CONDITION_SCHEMA = validate_recursive_condition


@CONDITION_REGISTRY.register(CONF_OR, OR_CONDITION_SCHEMA)
def or_condition_to_code(config, condition_id, arg_type, template_arg):
    for conditions in build_conditions(config, arg_type):
        yield
    rhs = OrCondition.new(template_arg, conditions)
    type = OrCondition.template(template_arg)
    yield Pvariable(condition_id, rhs, type=type)


RANGE_CONDITION_SCHEMA = vol.All(vol.Schema({
    vol.Optional(CONF_ABOVE): cv.templatable(cv.float_),
    vol.Optional(CONF_BELOW): cv.templatable(cv.float_),
}), cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW))


@CONDITION_REGISTRY.register(CONF_RANGE, RANGE_CONDITION_SCHEMA)
def range_condition_to_code(config, condition_id, arg_type, template_arg):
    for conditions in build_conditions(config, arg_type):
        yield
    rhs = RangeCondition.new(template_arg, conditions)
    type = RangeCondition.template(template_arg)
    condition = Pvariable(condition_id, rhs, type=type)
    if CONF_ABOVE in config:
        for template_ in templatable(config[CONF_ABOVE], arg_type, float_):
            yield
        condition.set_min(template_)
    if CONF_BELOW in config:
        for template_ in templatable(config[CONF_BELOW], arg_type, float_):
            yield
        condition.set_max(template_)
    yield condition


DELAY_ACTION_SCHEMA = cv.templatable(cv.positive_time_period_milliseconds)


@ACTION_REGISTRY.register(CONF_DELAY, DELAY_ACTION_SCHEMA)
def delay_action_to_code(config, action_id, arg_type, template_arg):
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
def if_action_to_code(config, action_id, arg_type, template_arg):
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


WHILE_ACTION_SCHEMA = vol.Schema({
    vol.Required(CONF_CONDITION): validate_recursive_condition,
    vol.Required(CONF_THEN): validate_recursive_action,
})


@ACTION_REGISTRY.register(CONF_WHILE, WHILE_ACTION_SCHEMA)
def while_action_to_code(config, action_id, arg_type, template_arg):
    for conditions in build_conditions(config[CONF_CONDITION], arg_type):
        yield None
    rhs = WhileAction.new(template_arg, conditions)
    type = WhileAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    for actions in build_actions(config[CONF_THEN], arg_type):
        yield None
    add(action.add_then(actions))
    yield action


def validate_wait_until(value):
    schema = vol.Schema({
        vol.Required(CONF_CONDITION): validate_recursive_condition
    })
    if isinstance(value, dict) and CONF_CONDITION in value:
        return schema(value)
    return validate_wait_until({CONF_CONDITION: value})


WAIT_UNTIL_ACTION_SCHEMA = validate_wait_until


@ACTION_REGISTRY.register(CONF_WAIT_UNTIL, WAIT_UNTIL_ACTION_SCHEMA)
def wait_until_action_to_code(config, action_id, arg_type, template_arg):
    for conditions in build_conditions(config[CONF_CONDITION], arg_type):
        yield None
    rhs = WaitUntilAction.new(template_arg, conditions)
    type = WaitUntilAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    add(App.register_component(action))
    yield action


LAMBDA_ACTION_SCHEMA = cv.lambda_


@ACTION_REGISTRY.register(CONF_LAMBDA, LAMBDA_ACTION_SCHEMA)
def lambda_action_to_code(config, action_id, arg_type, template_arg):
    for lambda_ in process_lambda(config, [(arg_type, 'x')], return_type=void):
        yield None
    rhs = LambdaAction.new(template_arg, lambda_)
    type = LambdaAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


LAMBDA_CONDITION_SCHEMA = cv.lambda_


@CONDITION_REGISTRY.register(CONF_LAMBDA, LAMBDA_CONDITION_SCHEMA)
def lambda_condition_to_code(config, condition_id, arg_type, template_arg):
    for lambda_ in process_lambda(config, [(arg_type, 'x')], return_type=bool_):
        yield
    rhs = LambdaCondition.new(template_arg, lambda_)
    type = LambdaCondition.template(template_arg)
    yield Pvariable(condition_id, rhs, type=type)


CONF_COMPONENT_UPDATE = 'component.update'
COMPONENT_UPDATE_ACTION_SCHEMA = maybe_simple_id({
    vol.Required(CONF_ID): cv.use_variable_id(PollingComponent),
})


@ACTION_REGISTRY.register(CONF_COMPONENT_UPDATE, COMPONENT_UPDATE_ACTION_SCHEMA)
def component_update_action_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = UpdateComponentAction.new(template_arg, var)
    type = UpdateComponentAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)


def build_action(full_config, arg_type):
    action_id = full_config[CONF_ACTION_ID]
    key, config = next((k, v) for k, v in full_config.items() if k in ACTION_REGISTRY)

    builder = ACTION_REGISTRY[key][1]
    template_arg = TemplateArguments(arg_type)
    for result in builder(config, action_id, arg_type, template_arg):
        yield None
    yield result


def build_actions(config, arg_type):
    actions = []
    for conf in config:
        for action in build_action(conf, arg_type):
            yield None
        actions.append(action)
    yield actions


def build_condition(full_config, arg_type):
    action_id = full_config[CONF_CONDITION_ID]
    key, config = next((k, v) for k, v in full_config.items() if k in CONDITION_REGISTRY)

    builder = CONDITION_REGISTRY[key][1]
    template_arg = TemplateArguments(arg_type)
    for result in builder(config, action_id, arg_type, template_arg):
        yield None
    yield result


def build_conditions(config, arg_type):
    conditions = []
    for conf in config:
        for condition in build_condition(conf, arg_type):
            yield None
        conditions.append(condition)
    yield conditions


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
    CORE.add_job(build_automation_, trigger, arg_type, config)
