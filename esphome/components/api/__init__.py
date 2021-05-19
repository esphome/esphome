import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import Condition
from esphome.const import (
    CONF_DATA,
    CONF_DATA_TEMPLATE,
    CONF_ID,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SERVICE,
    CONF_VARIABLES,
    CONF_SERVICES,
    CONF_TRIGGER_ID,
    CONF_EVENT,
    CONF_TAG,
)
from esphome.core import coroutine_with_priority

DEPENDENCIES = ["network"]
AUTO_LOAD = ["async_tcp"]
CODEOWNERS = ["@OttoWinter"]

api_ns = cg.esphome_ns.namespace("api")
APIServer = api_ns.class_("APIServer", cg.Component, cg.Controller)
HomeAssistantServiceCallAction = api_ns.class_(
    "HomeAssistantServiceCallAction", automation.Action
)
APIConnectedCondition = api_ns.class_("APIConnectedCondition", Condition)

UserServiceTrigger = api_ns.class_("UserServiceTrigger", automation.Trigger)
ListEntitiesServicesArgument = api_ns.class_("ListEntitiesServicesArgument")
SERVICE_ARG_NATIVE_TYPES = {
    "bool": bool,
    "int": cg.int32,
    "float": float,
    "string": cg.std_string,
    "bool[]": cg.std_vector.template(bool),
    "int[]": cg.std_vector.template(cg.int32),
    "float[]": cg.std_vector.template(float),
    "string[]": cg.std_vector.template(cg.std_string),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(APIServer),
        cv.Optional(CONF_PORT, default=6053): cv.port,
        cv.Optional(CONF_PASSWORD, default=""): cv.string_strict,
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="15min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SERVICES): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UserServiceTrigger),
                cv.Required(CONF_SERVICE): cv.valid_name,
                cv.Optional(CONF_VARIABLES, default={}): cv.Schema(
                    {
                        cv.validate_id_name: cv.one_of(
                            *SERVICE_ARG_NATIVE_TYPES, lower=True
                        ),
                    }
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    for conf in config.get(CONF_SERVICES, []):
        template_args = []
        func_args = []
        service_arg_names = []
        for name, var_ in conf[CONF_VARIABLES].items():
            native = SERVICE_ARG_NATIVE_TYPES[var_]
            template_args.append(native)
            func_args.append((native, name))
            service_arg_names.append(name)
        templ = cg.TemplateArguments(*template_args)
        trigger = cg.new_Pvariable(
            conf[CONF_TRIGGER_ID], templ, conf[CONF_SERVICE], service_arg_names
        )
        cg.add(var.register_user_service(trigger))
        yield automation.build_automation(trigger, func_args, conf)

    cg.add_define("USE_API")
    cg.add_global(api_ns.using)


KEY_VALUE_SCHEMA = cv.Schema({cv.string: cv.templatable(cv.string_strict)})

HOMEASSISTANT_SERVICE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(APIServer),
        cv.Required(CONF_SERVICE): cv.templatable(cv.string),
        cv.Optional(CONF_DATA, default={}): KEY_VALUE_SCHEMA,
        cv.Optional(CONF_DATA_TEMPLATE, default={}): KEY_VALUE_SCHEMA,
        cv.Optional(CONF_VARIABLES, default={}): cv.Schema(
            {cv.string: cv.returning_lambda}
        ),
    }
)


@automation.register_action(
    "homeassistant.service",
    HomeAssistantServiceCallAction,
    HOMEASSISTANT_SERVICE_ACTION_SCHEMA,
)
def homeassistant_service_to_code(config, action_id, template_arg, args):
    serv = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, False)
    templ = yield cg.templatable(config[CONF_SERVICE], args, None)
    cg.add(var.set_service(templ))
    for key, value in config[CONF_DATA].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_data(key, templ))
    for key, value in config[CONF_DATA_TEMPLATE].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_data_template(key, templ))
    for key, value in config[CONF_VARIABLES].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_variable(key, templ))
    yield var


def validate_homeassistant_event(value):
    value = cv.string(value)
    if not value.startswith("esphome."):
        raise cv.Invalid(
            "ESPHome can only generate Home Assistant events that begin with "
            "esphome. For example 'esphome.xyz'"
        )
    return value


HOMEASSISTANT_EVENT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(APIServer),
        cv.Required(CONF_EVENT): validate_homeassistant_event,
        cv.Optional(CONF_DATA, default={}): KEY_VALUE_SCHEMA,
        cv.Optional(CONF_DATA_TEMPLATE, default={}): KEY_VALUE_SCHEMA,
        cv.Optional(CONF_VARIABLES, default={}): KEY_VALUE_SCHEMA,
    }
)


@automation.register_action(
    "homeassistant.event",
    HomeAssistantServiceCallAction,
    HOMEASSISTANT_EVENT_ACTION_SCHEMA,
)
def homeassistant_event_to_code(config, action_id, template_arg, args):
    serv = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, True)
    templ = yield cg.templatable(config[CONF_EVENT], args, None)
    cg.add(var.set_service(templ))
    for key, value in config[CONF_DATA].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_data(key, templ))
    for key, value in config[CONF_DATA_TEMPLATE].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_data_template(key, templ))
    for key, value in config[CONF_VARIABLES].items():
        templ = yield cg.templatable(value, args, None)
        cg.add(var.add_variable(key, templ))
    yield var


HOMEASSISTANT_TAG_SCANNED_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(APIServer),
        cv.Required(CONF_TAG): cv.templatable(cv.string_strict),
    },
    key=CONF_TAG,
)


@automation.register_action(
    "homeassistant.tag_scanned",
    HomeAssistantServiceCallAction,
    HOMEASSISTANT_TAG_SCANNED_ACTION_SCHEMA,
)
def homeassistant_tag_scanned_to_code(config, action_id, template_arg, args):
    serv = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, True)
    cg.add(var.set_service("esphome.tag_scanned"))
    templ = yield cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.add_data("tag_id", templ))
    yield var


@automation.register_condition("api.connected", APIConnectedCondition, {})
def api_connected_to_code(config, condition_id, template_arg, args):
    yield cg.new_Pvariable(condition_id, template_arg)
