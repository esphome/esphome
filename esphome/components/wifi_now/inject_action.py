
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine
from esphome.components import binary_sensor 

from . import const as c
from . import cpp_types as t

INJECT_ACTION_SCHEMA = cv.Schema({
    cv.Required(c.CONF_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(c.CONF_PAYLOAD_ID): cv.use_id(t.TemplatePayloadSetter),
    })

@automation.register_action(c.ACTION_INJECT, t.InjectAction, INJECT_ACTION_SCHEMA)
@coroutine
def inject_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    binary_sensors_config = CORE.config[c.CONF_BINARY_SENSOR]
    for binary_sensor_conf in filter(lambda x: x[c.CONF_ID] == config[c.CONF_SENSOR_ID], binary_sensors_config):
        for conf in binary_sensor_conf.get(c.CONF_ON_PRESS, []):
            trigger = yield cg.get_variable(conf[c.CONF_TRIGGER_ID])
            cg.add(var.set_event_trigger( t.BinarySensorEvent.PRESS ,trigger))
        for conf in binary_sensor_conf.get(c.CONF_ON_RELEASE, []):
            trigger = yield cg.get_variable(conf[c.CONF_TRIGGER_ID])
            cg.add(var.set_event_trigger( t.BinarySensorEvent.RELEASE ,trigger))
        for conf in binary_sensor_conf.get(c.CONF_ON_CLICK, []):
            trigger = yield cg.get_variable(conf[c.CONF_TRIGGER_ID])
            cg.add(var.set_event_trigger( t.BinarySensorEvent.CLICK ,trigger))
        for conf in binary_sensor_conf.get(c.CONF_ON_DOUBLE_CLICK, []):
            trigger = yield cg.get_variable(conf[c.CONF_TRIGGER_ID])
            cg.add(var.set_event_trigger( t.BinarySensorEvent.DOUBLE_CLICK ,trigger))
        i = 1
        for conf in binary_sensor_conf.get(c.CONF_ON_MULTI_CLICK, []):
            trigger = yield cg.get_variable(conf[c.CONF_TRIGGER_ID])
            event = t.BINARY_SENSOR_EVENTS['multi_click_{}'.format(i)]
            cg.add(var.set_event_trigger( event, trigger))
            i += 1
            if i > 20:
                break
    payload_setter = yield cg.get_variable(config[c.CONF_PAYLOAD_ID])
    cg.add(var.set_payload_setter(payload_setter))
    yield var