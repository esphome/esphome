import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import CONF_ICON, CONF_ID, CONF_INTERNAL, CONF_ON_VALUE, \
    CONF_TRIGGER_ID, CONF_MQTT_ID, CONF_NAME
from esphome.core import CORE, coroutine

IS_PLATFORM_COMPONENT = True

# pylint: disable=invalid-name
text_sensor_ns = cg.esphome_ns.namespace('text_sensor')
TextSensor = text_sensor_ns.class_('TextSensor', cg.Nameable)
TextSensorPtr = TextSensor.operator('ptr')

TextSensorStateTrigger = text_sensor_ns.class_('TextSensorStateTrigger',
                                               cg.Trigger.template(cg.std_string))
TextSensorPublishAction = text_sensor_ns.class_('TextSensorPublishAction', cg.Action)

icon = cv.icon

TEXT_SENSOR_SCHEMA = cv.MQTT_COMPONENT_SCHEMA.extend({
    cv.OnlyWith(CONF_MQTT_ID, 'mqtt'): cv.declare_id(mqtt.MQTTTextSensor),
    cv.Optional(CONF_ICON): icon,
    cv.Optional(CONF_ON_VALUE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TextSensorStateTrigger),
    }),
})


@coroutine
def setup_text_sensor_core_(var, config):
    cg.add(var.set_name(config[CONF_NAME]))
    if CONF_INTERNAL in config:
        cg.add(var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(cg.std_string, 'x')], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        yield mqtt.register_mqtt_component(mqtt_, config)


@coroutine
def register_text_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text_sensor(var))
    yield setup_text_sensor_core_(var, config)


def to_code(config):
    cg.add_define('USE_TEXT_SENSOR')
    cg.add_global(text_sensor_ns.using)
