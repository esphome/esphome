import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_ID, CONF_NAME
from esphomeyaml.helpers import App, variable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('shutdown_switch'): cv.register_variable_id,
}).extend(switch.MQTT_SWITCH_SCHEMA.schema)


def to_code(config):
    rhs = App.make_shutdown_switch(config[CONF_NAME])
    shutdown = variable('Application::MakeShutdownSwitch', config[CONF_ID],
                        rhs)
    switch.setup_switch(shutdown.Pshutdown, config)
    switch.setup_mqtt_switch(shutdown.Pmqtt, config)


BUILD_FLAGS = '-DUSE_SHUTDOWN_SWITCH'
