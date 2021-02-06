import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID, CONF_NUM_ATTEMPTS, CONF_PASSWORD,
    CONF_PORT, CONF_REBOOT_TIMEOUT, CONF_SAFE_MODE
)
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ['@esphome/core']
DEPENDENCIES = ['network']

ota_ns = cg.esphome_ns.namespace('ota')
OTAComponent = ota_ns.class_('OTAComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OTAComponent),
    cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
    cv.SplitDefault(CONF_PORT, esp8266=8266, esp32=3232): cv.port,
    cv.Optional(CONF_PASSWORD, default=''): cv.string,
    cv.Optional(CONF_REBOOT_TIMEOUT, default='5min'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_NUM_ATTEMPTS, default='10'): cv.positive_not_null_int
}).extend(cv.COMPONENT_SCHEMA)


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
