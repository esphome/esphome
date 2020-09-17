import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import coroutine
from esphome.util import Registry

from . import const as c
from . import cpp_types as t

PAYLOAD_SETTER_REGISTRY = Registry(None, c.CONF_ID)

def register_payload_setter(name, payloadtype_type, schema):
    return PAYLOAD_SETTER_REGISTRY.register(name, payloadtype_type, schema)

validate_payload_setter_list = cv.validate_registry('payload', PAYLOAD_SETTER_REGISTRY)

@coroutine
def build_payload_setter(full_config, template_arg, args):
    registry_entry, config = cg.extract_registry_entry_config(PAYLOAD_SETTER_REGISTRY, full_config)
    builder = registry_entry.coroutine_fun
    yield builder(config, config[c.CONF_ID], template_arg, args)

@coroutine
def build_payload_setter_list(config, templ, arg_type):
    payloads = []
    for conf in config:
        payload = yield build_payload_setter(conf, templ, arg_type)
        payloads.append(payload)
    yield payloads

@coroutine
def templated_payload_setter_to_code(type, config, payload_id, template_arg, args):
    template_arg = cg.TemplateArguments(type, *[arg[0] for arg in args])
    var = cg.new_Pvariable(payload_id, template_arg, args)
    yield var

@register_payload_setter( c.PAYLOAD_BOOL, t.TemplatePayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.TemplatePayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def bool_payload_setter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_setter_to_code( cg.bool_, config, payload_id, template_arg, args)

@register_payload_setter( c.PAYLOAD_FLOAT, t.TemplatePayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.TemplatePayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def float_payload_setter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_setter_to_code( cg.float_, config, payload_id, template_arg, args)

@register_payload_setter( c.PAYLOAD_STRING, t.TemplatePayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.TemplatePayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def string_payload_setter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_setter_to_code( cg.std_string, config, payload_id, template_arg, args)

@register_payload_setter( c.PAYLOAD_VECTOR, t.TemplatePayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.TemplatePayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def vector_payload_setter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_setter_to_code( payload_t, config, payload_id, template_arg, args)

@register_payload_setter( c.PAYLOAD_BINARY_SENSOR_EVENT, t.TemplatePayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.TemplatePayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def binary_sensor_event_payload_setter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_setter_to_code( t.BinarySensorEvent, config, payload_id, template_arg, args)

@register_payload_setter( c.PAYLOAD_PAYLOAD, t.PayloadPayloadSetter, cv.maybe_simple_value( cv.Schema({
    cv.GenerateID(): cv.declare_id(t.PayloadPayloadSetter),
    }), key=c.CONF_ID))
@coroutine
def payload_payload_setter_to_code(config, payload_id, template_arg, args):
    var = cg.new_Pvariable(payload_id, template_arg, args)
    yield var
