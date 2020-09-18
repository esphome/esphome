import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as ehc
from esphome.core import coroutine
from esphome.util import Registry

from . import const as c
from . import cpp_types as t

PAYLOAD_GETTER_REGISTRY = Registry(None, ehc.CONF_ID)


def register_payload_getter(name, payloadtype_type, schema):
    return PAYLOAD_GETTER_REGISTRY.register(name, payloadtype_type, schema)


validate_payload_getter_list = cv.validate_registry('payload', PAYLOAD_GETTER_REGISTRY)


@coroutine
def build_payload_getter(full_config, template_arg, args):
    registry_entry, config = cg.extract_registry_entry_config(PAYLOAD_GETTER_REGISTRY, full_config)
    builder = registry_entry.coroutine_fun
    yield builder(config, config[ehc.CONF_ID], template_arg, args)


@coroutine
def build_payload_getter_list(config, templ, arg_type):
    payloads = []
    for conf in config:
        payload = yield build_payload_getter(conf, templ, arg_type)
        payloads.append(payload)
    yield payloads


@coroutine
def templated_payload_getter_to_code(type, config, payload_id, template_arg, args):
    template_arg = cg.TemplateArguments(type, *[arg[0] for arg in args])
    var = cg.new_Pvariable(payload_id, template_arg, args)
    template = yield cg.templatable(config[ehc.CONF_VALUE], args, type)
    cg.add(var.set_value(template))
    yield var


@register_payload_getter(c.PAYLOAD_BOOL, t.TemplatePayloadGetter, cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(t.TemplatePayloadGetter),
        cv.Required(ehc.CONF_VALUE): cv.templatable(cv.boolean),
        }), key=ehc.CONF_VALUE)
    )
@coroutine
def bool_payload_getter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_getter_to_code(cg.bool_, config, payload_id, template_arg, args)


@register_payload_getter(c.PAYLOAD_FLOAT, t.TemplatePayloadGetter, cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(t.TemplatePayloadGetter),
        cv.Required(ehc.CONF_VALUE): cv.templatable(cv.float_),
        }), key=ehc.CONF_VALUE)
    )
@coroutine
def float_payload_getter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_getter_to_code(cg.float_, config, payload_id, template_arg, args)


@register_payload_getter(c.PAYLOAD_STRING, t.TemplatePayloadGetter, cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(t.TemplatePayloadGetter),
        cv.Required(ehc.CONF_VALUE): cv.templatable(cv.string),
        }), key=ehc.CONF_VALUE)
    )
@coroutine
def string_payload_getter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_getter_to_code(cg.std_string, config, payload_id, template_arg, args)


@register_payload_getter(c.PAYLOAD_VECTOR, t.TemplatePayloadGetter, cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(t.TemplatePayloadGetter),
        cv.Required(ehc.CONF_VALUE): cv.templatable(cv.string),
        }), key=ehc.CONF_VALUE)
    )
@coroutine
def vector_payload_getter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_getter_to_code(t.payload_t, config, payload_id, template_arg, args)


@register_payload_getter(c.PAYLOAD_BINARY_SENSOR_EVENT, t.TemplatePayloadGetter, 
    cv.maybe_simple_value(
        cv.Schema({
            cv.GenerateID(): cv.declare_id(t.TemplatePayloadGetter),
            cv.Required(ehc.CONF_VALUE): 
                cv.templatable(cv.enum(t.BINARY_SENSOR_EVENTS, lower=True)
                ),
            }), 
            key=ehc.CONF_VALUE
        )
    )
@coroutine
def binary_sensor_event_payload_getter_to_code(config, payload_id, template_arg, args):
    yield templated_payload_getter_to_code(
        t.BinarySensorEvent, config,
        payload_id, template_arg, args
        )


@register_payload_getter(c.PAYLOAD_PAYLOAD, t.PayloadPayloadGetter, cv.maybe_simple_value(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(t.PayloadPayloadGetter),
        cv.Required(ehc.CONF_VALUE): cv.templatable(cv.string),
        }), key=ehc.CONF_VALUE)
    )
@coroutine
def payload_payload_getter_to_code(config, payload_id, template_arg, args):
    var = cg.new_Pvariable(payload_id, template_arg, args)
    template = yield cg.templatable(config[ehc.CONF_VALUE], args, t.payload_t)
    cg.add(var.set_value(template))
    yield var
