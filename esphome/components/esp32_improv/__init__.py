import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, output
from esphome.const import CONF_ID, ESP_PLATFORM_ESP32
from esphome.core import coroutine_with_priority


AUTO_LOAD = ["binary_sensor", "output"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32_ble_server", "wifi"]
ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_ACTIVATED_DURATION = "activated_duration"
CONF_ACTIVATOR = "activator"
CONF_IDENTIFY_DURATION = "identify_duration"
CONF_STATUS_INDICATOR = "status_indicator"
CONF_WIFI_TIMEOUT = "wifi_timeout"

esp32_improv_ns = cg.esphome_ns.namespace("esp32_improv")
ESP32ImprovComponent = esp32_improv_ns.class_("ESP32ImprovComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32ImprovComponent),
        cv.Optional(CONF_ACTIVATOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_STATUS_INDICATOR): cv.use_id(output.BinaryOutput),
        cv.Optional(
            CONF_IDENTIFY_DURATION, default="10s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_ACTIVATED_DURATION, default="1min"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(65.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add_define("USE_IMPROV")

    cg.add(var.set_identify_duration(config[CONF_IDENTIFY_DURATION]))
    cg.add(var.set_activated_duration(config[CONF_ACTIVATED_DURATION]))

    if CONF_ACTIVATOR in config:
        activator = yield cg.get_variable(config[CONF_ACTIVATOR])
        cg.add(var.set_activator(activator))

    if CONF_STATUS_INDICATOR in config:
        status_indicator = yield cg.get_variable(config[CONF_STATUS_INDICATOR])
        cg.add(var.set_status_indicator(status_indicator))
