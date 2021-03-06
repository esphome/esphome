import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_COUNTER, DEVICE_CLASS_EMPTY, \
    CONF_FREE, CONF_FRAGMENTATION, CONF_BLOCK, \
    CONF_ARDUINO_VERSION, ARDUINO_VERSION_ESP8266
from esphome.core import CORE
from esphome.core_config import PLATFORMIO_ESP8266_LUT

heap_sensor_ns = cg.esphome_ns.namespace('heap')
HeapSensor = heap_sensor_ns.class_('HeapSensor', cg.PollingComponent)


def _finditem(obj, key):
    if key in obj:
        return obj[key]
    for v in obj.values():
        if isinstance(v, dict):
            item = _finditem(v, key)
            if item is not None:
                return item
    return None


def validate_framework(config):
    if CORE.is_esp32:
        return config

    version = _finditem(CORE.raw_config, CONF_ARDUINO_VERSION)

    if version in [None, 'RECOMMENDED', 'LATEST', 'DEV']:
        return config

    framework = PLATFORMIO_ESP8266_LUT[version] if version in PLATFORMIO_ESP8266_LUT else version
    if framework < ARDUINO_VERSION_ESP8266['2.5.2']:
        raise cv.Invalid('This component is not supported on arduino framework version below 2.5.2')
    return config


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HeapSensor),
    cv.Optional(CONF_FREE): sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY),
    cv.Optional(CONF_FRAGMENTATION): cv.All(
        validate_framework,
        cv.only_on_esp8266,
        sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY)
    ),
    cv.Optional(CONF_BLOCK): cv.All(
        validate_framework,
        cv.only_on_esp8266,
        sensor.sensor_schema('Number', ICON_COUNTER, 0, DEVICE_CLASS_EMPTY)
    ),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    if CONF_FREE in config:
        sens = yield sensor.new_sensor(config[CONF_FREE])
        cg.add(var.set_free_sensor(sens))

    if CONF_FRAGMENTATION in config:
        sens = yield sensor.new_sensor(config[CONF_FRAGMENTATION])
        cg.add(var.set_fragmentation_sensor(sens))

    if CONF_BLOCK in config:
        sens = yield sensor.new_sensor(config[CONF_BLOCK])
        cg.add(var.set_block_sensor(sens))
