import base64

from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_ACTIONS,
    CONF_DATA,
    CONF_DATA_TEMPLATE,
    CONF_EVENT,
    CONF_ID,
    CONF_KEY,
    CONF_ON_CLIENT_CONNECTED,
    CONF_ON_CLIENT_DISCONNECTED,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SERVICE,
    CONF_SERVICES,
    CONF_TAG,
    CONF_TRIGGER_ID,
    CONF_VARIABLES,
)
from esphome.core import coroutine_with_priority

DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]
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
CONF_ENCRYPTION = "encryption"


def validate_encryption_key(value):
    value = cv.string_strict(value)
    try:
        decoded = base64.b64decode(value, validate=True)
    except ValueError as err:
        raise cv.Invalid("Invalid key format, please check it's using base64") from err

    if len(decoded) != 32:
        raise cv.Invalid("Encryption key must be base64 and 32 bytes long")

    # Return original data for roundtrip conversion
    return value


ACTIONS_SCHEMA = automation.validate_automation(
    {
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(UserServiceTrigger),
        cv.Exclusive(CONF_SERVICE, group_of_exclusion=CONF_ACTION): cv.valid_name,
        cv.Exclusive(CONF_ACTION, group_of_exclusion=CONF_ACTION): cv.valid_name,
        cv.Optional(CONF_VARIABLES, default={}): cv.Schema(
            {
                cv.validate_id_name: cv.one_of(*SERVICE_ARG_NATIVE_TYPES, lower=True),
            }
        ),
    },
    cv.All(
        cv.has_exactly_one_key(CONF_SERVICE, CONF_ACTION),
        cv.rename_key(CONF_SERVICE, CONF_ACTION),
    ),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APIServer),
            cv.Optional(CONF_PORT, default=6053): cv.port,
            cv.Optional(CONF_PASSWORD, default=""): cv.string_strict,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.Exclusive(
                CONF_SERVICES, group_of_exclusion=CONF_ACTIONS
            ): ACTIONS_SCHEMA,
            cv.Exclusive(CONF_ACTIONS, group_of_exclusion=CONF_ACTIONS): ACTIONS_SCHEMA,
            cv.Optional(CONF_ENCRYPTION): cv.Schema(
                {
                    cv.Required(CONF_KEY): validate_encryption_key,
                }
            ),
            cv.Optional(CONF_ON_CLIENT_CONNECTED): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_CLIENT_DISCONNECTED): automation.validate_automation(
                single=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.rename_key(CONF_SERVICES, CONF_ACTIONS),
)


@coroutine_with_priority(40.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    for conf in config.get(CONF_ACTIONS, []):
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
            conf[CONF_TRIGGER_ID], templ, conf[CONF_ACTION], service_arg_names
        )
        cg.add(var.register_user_service(trigger))
        await automation.build_automation(trigger, func_args, conf)

    if CONF_ON_CLIENT_CONNECTED in config:
        await automation.build_automation(
            var.get_client_connected_trigger(),
            [(cg.std_string, "client_info"), (cg.std_string, "client_address")],
            config[CONF_ON_CLIENT_CONNECTED],
        )

    if CONF_ON_CLIENT_DISCONNECTED in config:
        await automation.build_automation(
            var.get_client_disconnected_trigger(),
            [(cg.std_string, "client_info"), (cg.std_string, "client_address")],
            config[CONF_ON_CLIENT_DISCONNECTED],
        )

    if encryption_config := config.get(CONF_ENCRYPTION):
        decoded = base64.b64decode(encryption_config[CONF_KEY])
        cg.add(var.set_noise_psk(list(decoded)))
        cg.add_define("USE_API_NOISE")
        cg.add_library("esphome/noise-c", "0.1.6")
    else:
        cg.add_define("USE_API_PLAINTEXT")

    cg.add_define("USE_API")
    cg.add_global(api_ns.using)


KEY_VALUE_SCHEMA = cv.Schema({cv.string: cv.templatable(cv.string_strict)})


HOMEASSISTANT_ACTION_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(APIServer),
            cv.Exclusive(CONF_SERVICE, group_of_exclusion=CONF_ACTION): cv.templatable(
                cv.string
            ),
            cv.Exclusive(CONF_ACTION, group_of_exclusion=CONF_ACTION): cv.templatable(
                cv.string
            ),
            cv.Optional(CONF_DATA, default={}): KEY_VALUE_SCHEMA,
            cv.Optional(CONF_DATA_TEMPLATE, default={}): KEY_VALUE_SCHEMA,
            cv.Optional(CONF_VARIABLES, default={}): cv.Schema(
                {cv.string: cv.returning_lambda}
            ),
        }
    ),
    cv.has_exactly_one_key(CONF_SERVICE, CONF_ACTION),
    cv.rename_key(CONF_SERVICE, CONF_ACTION),
)


@automation.register_action(
    "homeassistant.action",
    HomeAssistantServiceCallAction,
    HOMEASSISTANT_ACTION_ACTION_SCHEMA,
)
@automation.register_action(
    "homeassistant.service",
    HomeAssistantServiceCallAction,
    HOMEASSISTANT_ACTION_ACTION_SCHEMA,
)
async def homeassistant_service_to_code(config, action_id, template_arg, args):
    serv = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, False)
    templ = await cg.templatable(config[CONF_ACTION], args, None)
    cg.add(var.set_service(templ))
    for key, value in config[CONF_DATA].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_data(key, templ))
    for key, value in config[CONF_DATA_TEMPLATE].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_data_template(key, templ))
    for key, value in config[CONF_VARIABLES].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_variable(key, templ))
    return var


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
async def homeassistant_event_to_code(config, action_id, template_arg, args):
    serv = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, True)
    templ = await cg.templatable(config[CONF_EVENT], args, None)
    cg.add(var.set_service(templ))
    for key, value in config[CONF_DATA].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_data(key, templ))
    for key, value in config[CONF_DATA_TEMPLATE].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_data_template(key, templ))
    for key, value in config[CONF_VARIABLES].items():
        templ = await cg.templatable(value, args, None)
        cg.add(var.add_variable(key, templ))
    return var


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
async def homeassistant_tag_scanned_to_code(config, action_id, template_arg, args):
    serv = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, serv, True)
    cg.add(var.set_service("esphome.tag_scanned"))
    templ = await cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.add_data("tag_id", templ))
    return var


@automation.register_condition("api.connected", APIConnectedCondition, {})
async def api_connected_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)
