import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID, CONF_NUM_ATTEMPTS, CONF_PASSWORD,
    CONF_PORT, CONF_REBOOT_TIMEOUT, CONF_SAFE_MODE
)
from esphome.core import CORE, coroutine_with_priority, TimePeriod

CODEOWNERS = ['@esphome/core']
DEPENDENCIES = ['network']

ota_ns = cg.esphome_ns.namespace('ota')
OTAComponent = ota_ns.class_('OTAComponent', cg.Component)


def validate(config):
    safe_mode_config = [CONF_NUM_ATTEMPTS, CONF_REBOOT_TIMEOUT]
    if any(config_option in config for config_option in safe_mode_config) and \
            not config[CONF_SAFE_MODE]:
        raise cv.Invalid(f"Cannot have {CONF_NUM_ATTEMPTS} or {CONF_REBOOT_TIMEOUT} "
                         f"without safe mode enabled!")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(OTAComponent),
    cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
    cv.SplitDefault(CONF_PORT, esp8266=8266, esp32=3232): cv.port,
    cv.Optional(CONF_PASSWORD, default=''): cv.string,
    cv.Optional(CONF_REBOOT_TIMEOUT, default='5min'):
      cv.All(cv.positive_time_period_milliseconds, cv.Range(max=TimePeriod(milliseconds=4294967295))),
    cv.Optional(CONF_NUM_ATTEMPTS, default='10'): cv.uint8_t
}).extend(cv.COMPONENT_SCHEMA), validate)


@coroutine_with_priority(50.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_auth_password(config[CONF_PASSWORD]))

    yield cg.register_component(var, config)

    if config[CONF_SAFE_MODE]:
        cg.add(var.start_safe_mode(config[CONF_NUM_ATTEMPTS], config[CONF_REBOOT_TIMEOUT]))

    if CORE.is_esp8266:
        cg.add_library('Update', None)
    elif CORE.is_esp32:
        cg.add_library('Hash', None)
