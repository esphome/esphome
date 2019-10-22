import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority, CORE

DEPENDENCIES = ['network']
AUTO_LOAD = ['async_tcp']

web_server_base_ns = cg.esphome_ns.namespace('web_server_base')
WebServerBase = web_server_base_ns.class_('WebServerBase', cg.Component)

CONF_WEB_SERVER_BASE_ID = 'web_server_base_id'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebServerBase),
})


@coroutine_with_priority(65.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CORE.is_esp32:
        cg.add_library('FS', None)
    # https://github.com/OttoWinter/ESPAsyncWebServer/blob/master/library.json
    cg.add_library('ESPAsyncWebServer-esphome', '1.2.5')
