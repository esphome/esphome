import esphome.codegen as cg
# from esphome.components import mqtt
# from esphome.components.mqtt import setup_mqtt_component
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ICON, CONF_ID, CONF_INTERNAL, CONF_ON_VALUE, \
    CONF_TRIGGER_ID
from esphome.core import CORE, coroutine

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

# pylint: disable=invalid-name
text_sensor_ns = cg.esphome_ns.namespace('text_sensor')
TextSensor = text_sensor_ns.class_('TextSensor', cg.Nameable)
TextSensorPtr = TextSensor.operator('ptr')
# MQTTTextSensor = text_sensor_ns.class_('MQTTTextSensor', mqtt.MQTTComponent)

TextSensorStateTrigger = text_sensor_ns.class_('TextSensorStateTrigger',
                                               cg.Trigger.template(cg.std_string))
TextSensorPublishAction = text_sensor_ns.class_('TextSensorPublishAction', cg.Action)

icon = cv.icon

TEXT_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    # cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTTextSensor),
    cv.Optional(CONF_ICON): icon,
    cv.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(TextSensorStateTrigger),
    }),
})

TEXT_SENSOR_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(TEXT_SENSOR_SCHEMA.schema)


@coroutine
def setup_text_sensor_core_(var, config):
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)

    # setup_mqtt_component(text_sensor_var.Pget_mqtt(), config)


@coroutine
def register_text_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text_sensor(var))
    yield setup_text_sensor_core_(var, config)


def to_code(config):
    cg.add_define('USE_TEXT_SENSOR')
