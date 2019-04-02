import voluptuous as vol

from esphome import automation
from esphome.automation import ACTION_REGISTRY, CONDITION_REGISTRY, Condition
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_DATA_TEMPLATE, CONF_ID, CONF_PASSWORD, CONF_PORT, \
    CONF_REBOOT_TIMEOUT, CONF_SERVICE, CONF_VARIABLES, CONF_SERVICES, CONF_TRIGGER_ID
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import Action, App, Component, StoringController, esphome_ns, Trigger, \
    bool_, int32, float_, std_string

api_ns = esphome_ns.namespace('api')
APIServer = api_ns.class_('APIServer', Component, StoringController)
HomeAssistantServiceCallAction = api_ns.class_('HomeAssistantServiceCallAction', Action)
KeyValuePair = api_ns.class_('KeyValuePair')
TemplatableKeyValuePair = api_ns.class_('TemplatableKeyValuePair')
APIConnectedCondition = api_ns.class_('APIConnectedCondition', Condition)

UserService = api_ns.class_('UserService', Trigger)
ServiceTypeArgument = api_ns.class_('ServiceTypeArgument')
ServiceArgType = api_ns.enum('ServiceArgType')
SERVICE_ARG_TYPES = {
    'bool': ServiceArgType.SERVICE_ARG_TYPE_BOOL,
    'int': ServiceArgType.SERVICE_ARG_TYPE_INT,
    'float': ServiceArgType.SERVICE_ARG_TYPE_FLOAT,
    'string': ServiceArgType.SERVICE_ARG_TYPE_STRING,
}
SERVICE_ARG_NATIVE_TYPES = {
    'bool': bool_,
    'int': int32,
    'float': float_,
    'string': std_string,
}


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(APIServer),
    vol.Optional(CONF_PORT, default=6053): cv.port,
    vol.Optional(CONF_PASSWORD, default=''): cv.string_strict,
    vol.Optional(CONF_REBOOT_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_SERVICES): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(UserService),
        vol.Required(CONF_SERVICE): cv.valid_name,
        vol.Optional(CONF_VARIABLES, default={}): cv.Schema({
            cv.validate_id_name: cv.one_of(*SERVICE_ARG_TYPES, lower=True),
        }),
    }),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.init_api_server()
    api = Pvariable(config[CONF_ID], rhs)

    if config[CONF_PORT] != 6053:
        add(api.set_port(config[CONF_PORT]))
    if config.get(CONF_PASSWORD):
        add(api.set_password(config[CONF_PASSWORD]))
    if CONF_REBOOT_TIMEOUT in config:
        add(api.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    for conf in config.get(CONF_SERVICES, []):
        template_args = []
        func_args = []
        service_type_args = []
        for name, var_ in conf[CONF_VARIABLES].items():
            native = SERVICE_ARG_NATIVE_TYPES[var_]
            template_args.append(native)
            func_args.append((native, name))
            service_type_args.append(ServiceTypeArgument(name, SERVICE_ARG_TYPES[var_]))
        func = api.make_user_service_trigger.template(*template_args)
        rhs = func(conf[CONF_SERVICE], service_type_args)
        type_ = UserService.template(*template_args)
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs, type=type_)
        automation.build_automations(trigger, func_args, conf)

    setup_component(api, config)


BUILD_FLAGS = '-DUSE_API'


def lib_deps(config):
    if CORE.is_esp32:
        return 'AsyncTCP@1.0.3'
    if CORE.is_esp8266:
        return 'ESPAsyncTCP@1.2.0'
    raise NotImplementedError


CONF_HOMEASSISTANT_SERVICE = 'homeassistant.service'
HOMEASSISTANT_SERVIC_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_variable_id(APIServer),
    vol.Required(CONF_SERVICE): cv.string,
    vol.Optional(CONF_DATA): cv.Schema({
        cv.string: cv.string,
    }),
    vol.Optional(CONF_DATA_TEMPLATE): cv.Schema({
        cv.string: cv.string,
    }),
    vol.Optional(CONF_VARIABLES): cv.Schema({
        cv.string: cv.lambda_,
    }),
})


@ACTION_REGISTRY.register(CONF_HOMEASSISTANT_SERVICE, HOMEASSISTANT_SERVIC_ACTION_SCHEMA)
def homeassistant_service_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_home_assistant_service_call_action(template_arg)
    type = HomeAssistantServiceCallAction.template(template_arg)
    act = Pvariable(action_id, rhs, type=type)
    add(act.set_service(config[CONF_SERVICE]))
    if CONF_DATA in config:
        datas = [KeyValuePair(k, v) for k, v in config[CONF_DATA].items()]
        add(act.set_data(datas))
    if CONF_DATA_TEMPLATE in config:
        datas = [KeyValuePair(k, v) for k, v in config[CONF_DATA_TEMPLATE].items()]
        add(act.set_data_template(datas))
    if CONF_VARIABLES in config:
        datas = []
        for key, value in config[CONF_VARIABLES].items():
            for value_ in process_lambda(value, []):
                yield None
            datas.append(TemplatableKeyValuePair(key, value_))
        add(act.set_variables(datas))
    yield act


CONF_API_CONNECTED = 'api.connected'
API_CONNECTED_CONDITION_SCHEMA = cv.Schema({})


@CONDITION_REGISTRY.register(CONF_API_CONNECTED, API_CONNECTED_CONDITION_SCHEMA)
def api_connected_to_code(config, condition_id, template_arg, args):
    rhs = APIConnectedCondition.new(template_arg)
    type = APIConnectedCondition.template(template_arg)
    yield Pvariable(condition_id, rhs, type=type)
