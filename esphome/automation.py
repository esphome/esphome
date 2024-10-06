import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTOMATION_ID,
    CONF_CONDITION,
    CONF_COUNT,
    CONF_ELSE,
    CONF_ID,
    CONF_THEN,
    CONF_TIME,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
    CONF_UPDATE_INTERVAL,
)
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.util import Registry


def maybe_simple_id(*validators):
    """Allow a raw ID to be specified in place of a config block.
    If the value that's being validated is a dictionary, it's passed as-is to the specified validators. Otherwise, it's
    wrapped in a dict that looks like ``{"id": <value>}``, and that dict is then handed off to the specified validators.
    """
    return maybe_conf(CONF_ID, *validators)


def maybe_conf(conf, *validators):
    """Allow a raw value to be specified in place of a config block.
    If the value that's being validated is a dictionary, it's passed as-is to the specified validators. Otherwise, it's
    wrapped in a dict that looks like ``{<conf>: <value>}``, and that dict is then handed off to the specified
    validators.
    (This is a general case of ``maybe_simple_id`` that allows the wrapping key to be something other than ``id``.)
    """
    validator = cv.All(*validators)

    @schema_extractor("maybe")
    def validate(value):
        if value == SCHEMA_EXTRACT:
            return (validator, conf)

        if isinstance(value, dict):
            return validator(value)
        with cv.remove_prepend_path([conf]):
            return validator({conf: value})

    return validate


def register_action(name, action_type, schema):
    return ACTION_REGISTRY.register(name, action_type, schema)


def register_condition(name, condition_type, schema):
    return CONDITION_REGISTRY.register(name, condition_type, schema)


Action = cg.esphome_ns.class_("Action")
Trigger = cg.esphome_ns.class_("Trigger")
ACTION_REGISTRY = Registry()
Condition = cg.esphome_ns.class_("Condition")
CONDITION_REGISTRY = Registry()
validate_action = cv.validate_registry_entry("action", ACTION_REGISTRY)
validate_action_list = cv.validate_registry("action", ACTION_REGISTRY)
validate_condition = cv.validate_registry_entry("condition", CONDITION_REGISTRY)
validate_condition_list = cv.validate_registry("condition", CONDITION_REGISTRY)


def validate_potentially_and_condition(value):
    if isinstance(value, list):
        with cv.remove_prepend_path(["and"]):
            return validate_condition({"and": value})
    return validate_condition(value)


DelayAction = cg.esphome_ns.class_("DelayAction", Action, cg.Component)
LambdaAction = cg.esphome_ns.class_("LambdaAction", Action)
IfAction = cg.esphome_ns.class_("IfAction", Action)
WhileAction = cg.esphome_ns.class_("WhileAction", Action)
RepeatAction = cg.esphome_ns.class_("RepeatAction", Action)
WaitUntilAction = cg.esphome_ns.class_("WaitUntilAction", Action, cg.Component)
UpdateComponentAction = cg.esphome_ns.class_("UpdateComponentAction", Action)
SuspendComponentAction = cg.esphome_ns.class_("SuspendComponentAction", Action)
ResumeComponentAction = cg.esphome_ns.class_("ResumeComponentAction", Action)
Automation = cg.esphome_ns.class_("Automation")

LambdaCondition = cg.esphome_ns.class_("LambdaCondition", Condition)
ForCondition = cg.esphome_ns.class_("ForCondition", Condition, cg.Component)


def validate_automation(extra_schema=None, extra_validators=None, single=False):
    if extra_schema is None:
        extra_schema = {}
    if isinstance(extra_schema, cv.Schema):
        extra_schema = extra_schema.schema
    schema = AUTOMATION_SCHEMA.extend(extra_schema)

    def validator_(value):
        if isinstance(value, list):
            # List of items, there are two possible options here, either a sequence of
            # actions (no then:) or a list of automations.
            try:
                # First try as a sequence of actions
                # If that succeeds, return immediately
                with cv.remove_prepend_path([CONF_THEN]):
                    return [schema({CONF_THEN: value})]
            except cv.Invalid as err:
                # Next try as a sequence of automations
                try:
                    return cv.Schema([schema])(value)
                except cv.Invalid as err2:
                    if "extra keys not allowed" in str(err2) and len(err2.path) == 2:
                        # pylint: disable=raise-missing-from
                        raise err
                    if "Unable to find action" in str(err):
                        raise err2
                    raise cv.MultipleInvalid([err, err2])
        elif isinstance(value, dict):
            if CONF_THEN in value:
                return [schema(value)]
            with cv.remove_prepend_path([CONF_THEN]):
                return [schema({CONF_THEN: value})]
        # This should only happen with invalid configs, but let's have a nice error message.
        return [schema(value)]

    @schema_extractor("automation")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return schema

        value = validator_(value)
        if extra_validators is not None:
            value = cv.Schema([extra_validators])(value)
        if single:
            if len(value) != 1:
                raise cv.Invalid("Cannot have more than 1 automation for templates")
            return value[0]
        return value

    return validator


AUTOMATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger),
        cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_id(Automation),
        cv.Required(CONF_THEN): validate_action_list,
    }
)

AndCondition = cg.esphome_ns.class_("AndCondition", Condition)
OrCondition = cg.esphome_ns.class_("OrCondition", Condition)
NotCondition = cg.esphome_ns.class_("NotCondition", Condition)
XorCondition = cg.esphome_ns.class_("XorCondition", Condition)


@register_condition("and", AndCondition, validate_condition_list)
async def and_condition_to_code(config, condition_id, template_arg, args):
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, conditions)


@register_condition("or", OrCondition, validate_condition_list)
async def or_condition_to_code(config, condition_id, template_arg, args):
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, conditions)


@register_condition("not", NotCondition, validate_potentially_and_condition)
async def not_condition_to_code(config, condition_id, template_arg, args):
    condition = await build_condition(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, condition)


@register_condition("xor", XorCondition, validate_condition_list)
async def xor_condition_to_code(config, condition_id, template_arg, args):
    conditions = await build_condition_list(config, template_arg, args)
    return cg.new_Pvariable(condition_id, template_arg, conditions)


@register_condition("lambda", LambdaCondition, cv.returning_lambda)
async def lambda_condition_to_code(config, condition_id, template_arg, args):
    lambda_ = await cg.process_lambda(config, args, return_type=bool)
    return cg.new_Pvariable(condition_id, template_arg, lambda_)


@register_condition(
    "for",
    ForCondition,
    cv.Schema(
        {
            cv.Required(CONF_TIME): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Required(CONF_CONDITION): validate_potentially_and_condition,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)
async def for_condition_to_code(config, condition_id, template_arg, args):
    condition = await build_condition(
        config[CONF_CONDITION], cg.TemplateArguments(), []
    )
    var = cg.new_Pvariable(condition_id, template_arg, condition)
    await cg.register_component(var, config)
    templ = await cg.templatable(config[CONF_TIME], args, cg.uint32)
    cg.add(var.set_time(templ))
    return var


@register_action(
    "delay", DelayAction, cv.templatable(cv.positive_time_period_milliseconds)
)
async def delay_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_component(var, {})
    template_ = await cg.templatable(config, args, cg.uint32)
    cg.add(var.set_delay(template_))
    return var


@register_action(
    "if",
    IfAction,
    cv.All(
        {
            cv.Required(CONF_CONDITION): validate_potentially_and_condition,
            cv.Optional(CONF_THEN): validate_action_list,
            cv.Optional(CONF_ELSE): validate_action_list,
        },
        cv.has_at_least_one_key(CONF_THEN, CONF_ELSE),
    ),
)
async def if_action_to_code(config, action_id, template_arg, args):
    conditions = await build_condition(config[CONF_CONDITION], template_arg, args)
    var = cg.new_Pvariable(action_id, template_arg, conditions)
    if CONF_THEN in config:
        actions = await build_action_list(config[CONF_THEN], template_arg, args)
        cg.add(var.add_then(actions))
    if CONF_ELSE in config:
        actions = await build_action_list(config[CONF_ELSE], template_arg, args)
        cg.add(var.add_else(actions))
    return var


@register_action(
    "while",
    WhileAction,
    cv.Schema(
        {
            cv.Required(CONF_CONDITION): validate_potentially_and_condition,
            cv.Required(CONF_THEN): validate_action_list,
        }
    ),
)
async def while_action_to_code(config, action_id, template_arg, args):
    conditions = await build_condition(config[CONF_CONDITION], template_arg, args)
    var = cg.new_Pvariable(action_id, template_arg, conditions)
    actions = await build_action_list(config[CONF_THEN], template_arg, args)
    cg.add(var.add_then(actions))
    return var


@register_action(
    "repeat",
    RepeatAction,
    cv.Schema(
        {
            cv.Required(CONF_COUNT): cv.templatable(cv.positive_not_null_int),
            cv.Required(CONF_THEN): validate_action_list,
        }
    ),
)
async def repeat_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    count_template = await cg.templatable(config[CONF_COUNT], args, cg.uint32)
    cg.add(var.set_count(count_template))
    actions = await build_action_list(
        config[CONF_THEN],
        cg.TemplateArguments(cg.uint32, *template_arg.args),
        [(cg.uint32, "iteration"), *args],
    )
    cg.add(var.add_then(actions))
    return var


_validate_wait_until = cv.maybe_simple_value(
    {
        cv.Required(CONF_CONDITION): validate_potentially_and_condition,
        cv.Optional(CONF_TIMEOUT): cv.templatable(cv.positive_time_period_milliseconds),
    },
    key=CONF_CONDITION,
)


@register_action("wait_until", WaitUntilAction, _validate_wait_until)
async def wait_until_action_to_code(config, action_id, template_arg, args):
    conditions = await build_condition(config[CONF_CONDITION], template_arg, args)
    var = cg.new_Pvariable(action_id, template_arg, conditions)
    if CONF_TIMEOUT in config:
        template_ = await cg.templatable(config[CONF_TIMEOUT], args, cg.uint32)
        cg.add(var.set_timeout_value(template_))
    await cg.register_component(var, {})
    return var


@register_action("lambda", LambdaAction, cv.lambda_)
async def lambda_action_to_code(config, action_id, template_arg, args):
    lambda_ = await cg.process_lambda(config, args, return_type=cg.void)
    return cg.new_Pvariable(action_id, template_arg, lambda_)


@register_action(
    "component.update",
    UpdateComponentAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.PollingComponent),
        }
    ),
)
async def component_update_action_to_code(config, action_id, template_arg, args):
    comp = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, comp)


@register_action(
    "component.suspend",
    SuspendComponentAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.PollingComponent),
        }
    ),
)
async def component_suspend_action_to_code(config, action_id, template_arg, args):
    comp = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, comp)


@register_action(
    "component.resume",
    ResumeComponentAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(cg.PollingComponent),
            cv.Optional(CONF_UPDATE_INTERVAL): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
)
async def component_resume_action_to_code(config, action_id, template_arg, args):
    comp = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, comp)
    if CONF_UPDATE_INTERVAL in config:
        template_ = await cg.templatable(config[CONF_UPDATE_INTERVAL], args, int)
        cg.add(var.set_update_interval(template_))
    return var


async def build_action(full_config, template_arg, args):
    registry_entry, config = cg.extract_registry_entry_config(
        ACTION_REGISTRY, full_config
    )
    action_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    ret = await builder(config, action_id, template_arg, args)
    return ret


async def build_action_list(config, templ, arg_type):
    actions = []
    for conf in config:
        action = await build_action(conf, templ, arg_type)
        actions.append(action)
    return actions


async def build_condition(full_config, template_arg, args):
    registry_entry, config = cg.extract_registry_entry_config(
        CONDITION_REGISTRY, full_config
    )
    action_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    ret = await builder(config, action_id, template_arg, args)
    return ret


async def build_condition_list(config, templ, args):
    conditions = []
    for conf in config:
        condition = await build_condition(conf, templ, args)
        conditions.append(condition)
    return conditions


async def build_automation(trigger, args, config):
    arg_types = [arg[0] for arg in args]
    templ = cg.TemplateArguments(*arg_types)
    obj = cg.new_Pvariable(config[CONF_AUTOMATION_ID], templ, trigger)
    actions = await build_action_list(config[CONF_THEN], templ, args)
    cg.add(obj.add_actions(actions))
    return obj
