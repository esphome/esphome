import esphome.config_validation as cv
import esphome.codegen as cg

CONF_IMPLEMENTATION = 'implementation'
IMPLEMENTATIONS = [
    'ESP8266_WIFI_CLIENT',
    'LWIP_RAW_TCP',
    'ESP32_WIFI_CLIENT',
    'UNIX_SOCKET',
    'ASYNC_TCP',
]

CONFIG_SCHEMA = cv.Schema({
    cv.SplitDefault(CONF_IMPLEMENTATION, esp8266='LWIP_RAW_TCP', esp32='UNIX_SOCKET'):
        cv.one_of(*IMPLEMENTATIONS, upper=True, space='_'),
})


def to_code(config):
    implementation = config[CONF_IMPLEMENTATION]
    cg.add_define('USE_TCP_{}'.format(implementation))
