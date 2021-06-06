import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome import automation
from esphome.core import coroutine

CODEOWNERS = ['@wifwucite']

### Shared stuff ########################################################################################################

esp32_ble_controller_ns = cg.esphome_ns.namespace('esp32_ble_controller')
ESP32BLEController = esp32_ble_controller_ns.class_('ESP32BLEController', cg.Component, cg.Controller)

### Configuration validation ############################################################################################

# BLE services and characteristics #####
CONF_BLE_SERVICES = "services"
CONF_BLE_SERVICE = "service"
CONF_BLE_CHARACTERISTICS = "characteristics"
CONF_BLE_CHARACTERISTIC = "characteristic"
CONF_BLE_USE_2902 = "use_BLE2902"
CONF_EXPOSES_COMPONENT = "exposes"

def validate_UUID(value):
    # print("UUID«", value)
    value = cv.string(value)
    # TASK improve the regex
    if re.match(r'^[0-9a-fA-F\-]{8,36}$', value) is None:
        raise cv.Invalid("valid UUID required")
    return value

BLE_CHARACTERISTIC = cv.Schema({
    cv.Required("characteristic"): validate_UUID,
    cv.GenerateID(CONF_EXPOSES_COMPONENT): cv.use_id(cg.Nameable), # TASK validate that only supported Nameables are referenced
    cv.Optional(CONF_BLE_USE_2902, default=True): cv.boolean,
})

BLE_SERVICE = cv.Schema({
    cv.Required(CONF_BLE_SERVICE): validate_UUID,
    cv.Required(CONF_BLE_CHARACTERISTICS): cv.ensure_list(BLE_CHARACTERISTIC),
})

# custom commands #####
CONF_BLE_COMMANDS = "commands"
CONF_BLE_CMD_ID = "command"
CONF_BLE_CMD_DESCRIPTION = "description"
CONF_BLE_CMD_ON_EXECUTE = "on_execute"
BLEControllerCustomCommandExecutionTrigger = esp32_ble_controller_ns.class_('BLEControllerCustomCommandExecutionTrigger', automation.Trigger.template())

BUILTIN_CMD_IDS = ['help', 'ble-services', 'wifi-config', 'log-level']
CMD_ID_CHARACTERS = "abcdefghijklmnopqrstuvwxyz0123456789-"
def validate_command_id(value):
    """Validate that this value is a valid command id.
    """
    value = cv.string_strict(value).lower()
    if value in BUILTIN_CMD_IDS:
        raise cv.Invalid(f"{value} is a built-in command")
    for c in value:
        if c not in CMD_ID_CHARACTERS:
            raise cv.Invalid(f"Invalid character for command id: {c}")
    return value

BLE_COMMAND = cv.Schema({
    cv.Required(CONF_BLE_CMD_ID): validate_command_id,
    cv.Required(CONF_BLE_CMD_DESCRIPTION): cv.string_strict,
    cv.Required(CONF_BLE_CMD_ON_EXECUTE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEControllerCustomCommandExecutionTrigger),
    }),
})

# security mode enumeration #####
CONF_SECURITY_MODE = 'security_mode'
CONF_SECURITY_MODE_NONE = 'none'
CONF_SECURITY_MODE_SHOW_PASS_KEY = 'show_pass_key'
SECURTY_MODE_OPTIONS = {
    CONF_SECURITY_MODE_NONE: False,
    CONF_SECURITY_MODE_SHOW_PASS_KEY: True,
}

# authetication and (dis)connected automations #####
CONF_ON_SHOW_PASS_KEY = "on_show_pass_key"
BLEControllerShowPassKeyTrigger = esp32_ble_controller_ns.class_('BLEControllerShowPassKeyTrigger', automation.Trigger.template())

CONF_ON_AUTHENTICATION_COMPLETE = "on_authentication_complete"
BLEControllerAuthenticationCompleteTrigger = esp32_ble_controller_ns.class_('BLEControllerAuthenticationCompleteTrigger', automation.Trigger.template())

def require_config_setting_for_automation(automation_id, setting_key, required_setting_value, config):
    """Validates that a given automation is only present if a given setting has a given value."""
    if automation_id in config and config[setting_key] != required_setting_value:
        raise cv.Invalid("Automation '" + automation_id + "' only available if " + setting_key + " = " + required_setting_value)

def automations_available(config):
    """Validates that the pass key related automations are only present if the security mode is set to show a pass key."""
    require_config_setting_for_automation(CONF_ON_SHOW_PASS_KEY, CONF_SECURITY_MODE, CONF_SECURITY_MODE_SHOW_PASS_KEY, config)
    require_config_setting_for_automation(CONF_ON_AUTHENTICATION_COMPLETE, CONF_SECURITY_MODE, CONF_SECURITY_MODE_SHOW_PASS_KEY, config)
    return config

CONF_ON_SERVER_CONNECTED = "on_connected"
BLEControllerServerConnectedTrigger = esp32_ble_controller_ns.class_('BLEControllerServerConnectedTrigger', automation.Trigger.template())

CONF_ON_SERVER_DISCONNECTED = "on_disconnected"
BLEControllerServerDisconnectedTrigger = esp32_ble_controller_ns.class_('BLEControllerServerDisconnectedTrigger', automation.Trigger.template())

# Schema for the controller (incl. validation) #####
CONFIG_SCHEMA = cv.All(cv.only_on_esp32, cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32BLEController),

    cv.Optional(CONF_BLE_SERVICES): cv.ensure_list(BLE_SERVICE),

    cv.Optional(CONF_BLE_COMMANDS): cv.ensure_list(BLE_COMMAND),

    cv.Optional(CONF_SECURITY_MODE, default=CONF_SECURITY_MODE_SHOW_PASS_KEY): cv.enum(SECURTY_MODE_OPTIONS),

    cv.Optional(CONF_ON_SHOW_PASS_KEY): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEControllerShowPassKeyTrigger),
    }),
    cv.Optional(CONF_ON_AUTHENTICATION_COMPLETE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEControllerAuthenticationCompleteTrigger),
    }),

    cv.Optional(CONF_ON_SERVER_CONNECTED): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEControllerServerConnectedTrigger),
    }),
    cv.Optional(CONF_ON_SERVER_DISCONNECTED): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEControllerServerDisconnectedTrigger),
    }),

    }), automations_available)

### Code generation ############################################################################################

@coroutine
def to_code_characteristic(ble_controller_var, service_uuid, characteristic_description):
    """Coroutine that registers the given characteristic of the given service with BLE controller, 
    i.e. generates a single controller->register_component(...) call"""
    characteristic_uuid = characteristic_description[CONF_BLE_CHARACTERISTIC]
    component_id = characteristic_description[CONF_EXPOSES_COMPONENT]
    component = yield cg.get_variable(component_id)
    use_BLE2902 = characteristic_description[CONF_BLE_USE_2902]
    cg.add(ble_controller_var.register_component(component, service_uuid, characteristic_uuid, use_BLE2902))
    
@coroutine
def to_code_service(ble_controller_var, service):
    """Coroutine that registers all characteristics of the given service with BLE controller"""
    service_uuid = service[CONF_BLE_SERVICE]
    characteristics = service[CONF_BLE_CHARACTERISTICS]
    for characteristic_description in characteristics:
        yield to_code_characteristic(ble_controller_var, service_uuid, characteristic_description)

@coroutine
def to_code_command(ble_controller_var, cmd):
    """Coroutine that registers all BLE commands with BLE controller"""
    id = cmd[CONF_BLE_CMD_ID]
    description = cmd[CONF_BLE_CMD_DESCRIPTION]
    trigger_conf = cmd[CONF_BLE_CMD_ON_EXECUTE][0]
    trigger = cg.new_Pvariable(trigger_conf[CONF_TRIGGER_ID], ble_controller_var)
    yield automation.build_automation(trigger, [(cg.std_ns.class_("vector<std::string>"), 'arguments'), (esp32_ble_controller_ns.class_("BLECustomCommandResultHolder"), 'result')], trigger_conf)
    cg.add(ble_controller_var.register_command(id, description, trigger))

def to_code(config):
    """Generates the C++ code for the BLE controller configuration"""
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    for cmd in config.get(CONF_BLE_SERVICES, []):
        yield to_code_service(var, cmd)

    for cmd in config.get(CONF_BLE_COMMANDS, []):
        yield to_code_command(var, cmd)

    security_enabled = SECURTY_MODE_OPTIONS[config[CONF_SECURITY_MODE]]
    cg.add(var.set_security_enabled(config[CONF_SECURITY_MODE]))

    for conf in config.get(CONF_ON_SHOW_PASS_KEY, []):
        if security_enabled:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            yield automation.build_automation(trigger, [(cg.std_string, 'pass_key')], conf)

    for conf in config.get(CONF_ON_AUTHENTICATION_COMPLETE, []):
        if security_enabled:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            yield automation.build_automation(trigger, [(cg.bool_, 'success')], conf)

    for conf in config.get(CONF_ON_SERVER_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SERVER_DISCONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)
