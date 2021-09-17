#!/usr/bin/env python3

from esphome.cpp_generator import MockObj
import json
import argparse
import os
import re
from pathlib import Path
import voluptuous as vol

# NOTE: Cannot import other esphome components globally as a modification in jsonschema
# is needed before modules are loaded
import esphome.jsonschema as ejs

ejs.EnableJsonSchemaCollect = True

DUMP_COMMENTS = False

JSC_ACTION = "automation.ACTION_REGISTRY"
JSC_ALLOF = "allOf"
JSC_ANYOF = "anyOf"
JSC_COMMENT = "$comment"
JSC_CONDITION = "automation.CONDITION_REGISTRY"
JSC_DESCRIPTION = "description"
JSC_ONEOF = "oneOf"
JSC_PROPERTIES = "properties"
JSC_REF = "$ref"

# this should be required, but YAML Language server completion does not work properly if required are specified.
# still needed for other features / checks
JSC_REQUIRED = "required_"

SIMPLE_AUTOMATION = "simple_automation"

schema_names = {}
schema_registry = {}
components = {}
modules = {}
registries = []
pending_refs = []

definitions = {}
base_props = {}


parser = argparse.ArgumentParser()
parser.add_argument(
    "--output", default="esphome.json", help="Output filename", type=os.path.abspath
)

args = parser.parse_args()


def get_ref(definition):
    return {JSC_REF: "#/definitions/" + definition}


def is_ref(jschema):
    return isinstance(jschema, dict) and JSC_REF in jschema


def unref(jschema):
    return definitions.get(jschema[JSC_REF][len("#/definitions/") :])


def add_definition_array_or_single_object(ref):
    return {JSC_ANYOF: [{"type": "array", "items": ref}, ref]}


def add_core():
    from esphome.core.config import CONFIG_SCHEMA

    base_props["esphome"] = get_jschema("esphome", CONFIG_SCHEMA.schema)


def add_buses():
    # uart
    from esphome.components.uart import UART_DEVICE_SCHEMA

    get_jschema("uart_bus", UART_DEVICE_SCHEMA)

    # spi
    from esphome.components.spi import spi_device_schema

    get_jschema("spi_bus", spi_device_schema(False))

    # i2c
    from esphome.components.i2c import i2c_device_schema

    get_jschema("i2c_bus", i2c_device_schema(None))


def add_registries():
    for domain, module in modules.items():
        add_module_registries(domain, module)


def add_module_registries(domain, module):
    from esphome.util import Registry

    for c in dir(module):
        m = getattr(module, c)
        if isinstance(m, Registry):
            add_registry(domain + "." + c, m)


def add_registry(registry_name, registry):
    validators = []
    registries.append((registry, registry_name))
    for name in registry.keys():
        schema = get_jschema(str(name), registry[name].schema, create_return_ref=False)
        if not schema:
            schema = {"type": "null"}
        o_schema = {"type": "object", JSC_PROPERTIES: {name: schema}}
        o_schema = create_ref(
            registry_name + "-" + name, str(registry[name].schema) + "x", o_schema
        )
        validators.append(o_schema)
    definitions[registry_name] = {JSC_ANYOF: validators}


def get_registry_ref(registry):
    # we don't know yet
    ref = {JSC_REF: "pending"}
    pending_refs.append((ref, registry))
    return ref


def solve_pending_refs():
    for ref, registry in pending_refs:
        for registry_match, name in registries:
            if registry == registry_match:
                ref[JSC_REF] = "#/definitions/" + name


def add_module_schemas(name, module):
    import esphome.config_validation as cv

    for c in dir(module):
        v = getattr(module, c)
        if isinstance(v, cv.Schema):
            get_jschema(name + "." + c, v)


def get_dirs():
    from esphome.loader import CORE_COMPONENTS_PATH

    dir_names = [
        d
        for d in os.listdir(CORE_COMPONENTS_PATH)
        if not d.startswith("__")
        and os.path.isdir(os.path.join(CORE_COMPONENTS_PATH, d))
    ]
    return dir_names


def get_logger_tags():
    from esphome.loader import CORE_COMPONENTS_PATH
    import glob

    pattern = re.compile(r'^static const char(\*\s|\s\*)TAG = "(\w.*)";', re.MULTILINE)
    tags = [
        "app",
        "component",
        "esphal",
        "helpers",
        "preferences",
        "scheduler",
        "api.service",
    ]
    for x in os.walk(CORE_COMPONENTS_PATH):
        for y in glob.glob(os.path.join(x[0], "*.cpp")):
            with open(y, "r") as file:
                data = file.read()
                match = pattern.search(data)
                if match:
                    tags.append(match.group(2))
    return tags


def load_components():
    import esphome.config_validation as cv
    from esphome.config import get_component

    modules["cv"] = cv
    from esphome import automation

    modules["automation"] = automation

    for domain in get_dirs():
        components[domain] = get_component(domain)
        modules[domain] = components[domain].module


def add_components():
    from esphome.config import get_platform

    for domain, c in components.items():
        if c.is_platform_component:
            # this is a platform_component, e.g. binary_sensor
            platform_schema = [
                {
                    "type": "object",
                    "properties": {"platform": {"type": "string"}},
                }
            ]
            if domain not in ("output", "display"):
                # output bases are either FLOAT or BINARY so don't add common base for this
                # display bases are either simple or FULL so don't add common base for this
                platform_schema = [
                    {"$ref": f"#/definitions/{domain}.{domain.upper()}_SCHEMA"}
                ] + platform_schema

            base_props[domain] = {"type": "array", "items": {"allOf": platform_schema}}

            add_module_registries(domain, c.module)
            add_module_schemas(domain, c.module)

            # need first to iterate all platforms then iteate components
            # a platform component can have other components as properties,
            # e.g. climate components usually have a temperature sensor

    for domain, c in components.items():
        if (c.config_schema is not None) or c.is_platform_component:
            if c.is_platform_component:
                platform_schema = base_props[domain]["items"]["allOf"]
                for platform in get_dirs():
                    p = get_platform(domain, platform)
                    if p is not None:
                        # this is a platform element, e.g.
                        #   - platform: gpio
                        schema = get_jschema(
                            domain + "-" + platform,
                            p.config_schema,
                            create_return_ref=False,
                        )
                        if (
                            schema
                        ):  # for invalid schemas, None is returned thus is deprecated
                            platform_schema.append(
                                {
                                    "if": {
                                        JSC_PROPERTIES: {
                                            "platform": {"const": platform}
                                        }
                                    },
                                    "then": schema,
                                }
                            )

            elif c.config_schema is not None:
                # adds root components which are not platforms, e.g. api: logger:
                if c.multi_conf:
                    schema = get_jschema(domain, c.config_schema)
                    schema = add_definition_array_or_single_object(schema)
                else:
                    schema = get_jschema(domain, c.config_schema, False)
                base_props[domain] = schema


def get_automation_schema(name, vschema):
    from esphome.automation import AUTOMATION_SCHEMA

    # ensure SIMPLE_AUTOMATION
    if SIMPLE_AUTOMATION not in definitions:
        simple_automation = add_definition_array_or_single_object(get_ref(JSC_ACTION))
        simple_automation[JSC_ANYOF].append(
            get_jschema(AUTOMATION_SCHEMA.__module__, AUTOMATION_SCHEMA)
        )

        definitions[schema_names[str(AUTOMATION_SCHEMA)]][JSC_PROPERTIES][
            "then"
        ] = add_definition_array_or_single_object(get_ref(JSC_ACTION))
        definitions[SIMPLE_AUTOMATION] = simple_automation

    extra_vschema = None
    if AUTOMATION_SCHEMA == ejs.extended_schemas[str(vschema)][0]:
        extra_vschema = ejs.extended_schemas[str(vschema)][1]

    if not extra_vschema:
        return get_ref(SIMPLE_AUTOMATION)

    # add then property
    extra_jschema = get_jschema(name, extra_vschema, False)

    if is_ref(extra_jschema):
        return extra_jschema

    if not JSC_PROPERTIES in extra_jschema:
        # these are interval: and exposure_notifications, featuring automations a component
        extra_jschema[JSC_ALLOF][0][JSC_PROPERTIES][
            "then"
        ] = add_definition_array_or_single_object(get_ref(JSC_ACTION))
        ref = create_ref(name, extra_vschema, extra_jschema)
        return add_definition_array_or_single_object(ref)

    # automations can be either
    #   * a single action,
    #   * an array of action,
    #   * an object with automation's schema and a then key
    #        with again a single action or an array of actions

    if len(extra_jschema[JSC_PROPERTIES]) == 0:
        return get_ref(SIMPLE_AUTOMATION)

    extra_jschema[JSC_PROPERTIES]["then"] = add_definition_array_or_single_object(
        get_ref(JSC_ACTION)
    )
    # if there is a required element in extra_jschema then this automation does not support
    # directly a list of actions
    if JSC_REQUIRED in extra_jschema:
        return create_ref(name, extra_vschema, extra_jschema)

    jschema = add_definition_array_or_single_object(get_ref(JSC_ACTION))
    jschema[JSC_ANYOF].append(extra_jschema)

    return create_ref(name, extra_vschema, jschema)


def get_entry(parent_key, vschema):
    from esphome.voluptuous_schema import _Schema as schema_type

    entry = {}
    # annotate schema validator info
    if DUMP_COMMENTS:
        entry[JSC_COMMENT] = "entry: " + parent_key + "/" + str(vschema)

    if isinstance(vschema, list):
        ref = get_jschema(parent_key + "[]", vschema[0])
        entry = {"type": "array", "items": ref}
    elif isinstance(vschema, schema_type) and hasattr(vschema, "schema"):
        entry = get_jschema(parent_key, vschema, False)
    elif hasattr(vschema, "validators"):
        entry = get_jschema(parent_key, vschema, False)
    elif vschema in schema_registry:
        entry = schema_registry[vschema].copy()
    elif str(vschema) in ejs.registry_schemas:
        entry = get_registry_ref(ejs.registry_schemas[str(vschema)])
    elif str(vschema) in ejs.list_schemas:
        ref = get_jschema(parent_key, ejs.list_schemas[str(vschema)][0])
        entry = {JSC_ANYOF: [ref, {"type": "array", "items": ref}]}
    elif str(vschema) in ejs.typed_schemas:
        schema_types = [{"type": "object", "properties": {"type": {"type": "string"}}}]
        entry = {"allOf": schema_types}
        for schema_key, vschema_type in ejs.typed_schemas[str(vschema)][0][0].items():
            schema_types.append(
                {
                    "if": {"properties": {"type": {"const": schema_key}}},
                    "then": get_jschema(f"{parent_key}-{schema_key}", vschema_type),
                }
            )

    elif str(vschema) in ejs.hidden_schemas:
        # get the schema from the automation schema
        type = ejs.hidden_schemas[str(vschema)]
        inner_vschema = vschema(ejs.jschema_extractor)
        if type == "automation":
            entry = get_automation_schema(parent_key, inner_vschema)
        elif type == "maybe":
            entry = get_jschema(parent_key, inner_vschema)
        elif type == "one_of":
            entry = {"enum": list(inner_vschema)}
        elif type == "enum":
            entry = {"enum": list(inner_vschema.keys())}
        elif type == "effects":
            # Like list schema but subset from list.
            subset_list = inner_vschema[0]
            # get_jschema('strobex', registry['strobe'].schema)
            registry_schemas = []
            for name in subset_list:
                registry_schemas.append(get_ref("light.EFFECTS_REGISTRY-" + name))

            entry = {
                JSC_ANYOF: [{"type": "array", "items": {JSC_ANYOF: registry_schemas}}]
            }

        else:
            raise ValueError("Unknown extracted schema type")
    elif str(vschema).startswith("<function invalid."):
        # deprecated options, don't list as valid schema
        return None
    else:
        # everything else just accept string and let ESPHome validate
        try:
            from esphome.core import ID
            from esphome.automation import Trigger, Automation

            v = vschema(None)
            if isinstance(v, ID):
                if v.type.base != "script::Script" and (
                    v.type.inherits_from(Trigger) or v.type == Automation
                ):
                    return None
                entry = {"type": "string", "id_type": v.type.base}
            elif isinstance(v, str):
                entry = {"type": "string"}
            elif isinstance(v, list):
                entry = {"type": "array"}
            else:
                entry = default_schema()
        except:
            entry = default_schema()

    return entry


def default_schema():
    # Accept anything
    return {"type": ["null", "object", "string", "array", "number"]}


def is_default_schema(jschema):
    if is_ref(jschema):
        jschema = unref(jschema)
        if not jschema:
            return False
        return is_default_schema(jschema)
    return "type" in jschema and jschema["type"] == default_schema()["type"]


def get_jschema(path, vschema, create_return_ref=True):
    name = schema_names.get(get_schema_str(vschema))
    if name:
        return get_ref(name)

    jschema = convert_schema(path, vschema)

    if is_ref(jschema):
        # this can happen when returned extended
        # schemas where all properties found in previous extended schema
        return jschema

    if not create_return_ref:
        return jschema

    return create_ref(path, vschema, jschema)


def get_schema_str(vschema):
    # Hack on cs.use_id, in the future this can be improved by tracking which type is required by
    # the id, this information can be added somehow to schema (not supported by jsonschema) and
    # completion can be improved listing valid ids only Meanwhile it's a problem because it makes
    # all partial schemas with cv.use_id different, e.g. i2c

    return re.sub(
        pattern="function use_id.<locals>.validator at 0[xX][0-9a-fA-F]+>",
        repl="function use_id.<locals>.validator<>",
        string=str(vschema),
    )


def create_ref(name, vschema, jschema):
    if name in schema_names:
        raise ValueError("Not supported")

    schema_str = get_schema_str(vschema)

    schema_names[schema_str] = name
    definitions[name] = jschema
    return get_ref(name)


def get_all_properties(jschema):
    if JSC_PROPERTIES in jschema:
        return list(jschema[JSC_PROPERTIES].keys())
    if is_ref(jschema):
        return get_all_properties(unref(jschema))
    arr = jschema.get(JSC_ALLOF, jschema.get(JSC_ANYOF))
    props = []
    for x in arr:
        props = props + get_all_properties(x)

    return props


def merge(arr, element):
    # arr is an array of dicts, dicts can have keys like, properties, $ref, required:[], etc
    # element is a single dict which might have several keys to
    # the result should be an array with only one element containing properties, required, etc
    # and other elements for needed $ref elements
    # NOTE: json schema supports allof with properties in different elements, but that makes
    # complex for later adding docs to the schema
    for k, v in element.items():
        if k == JSC_PROPERTIES:
            props_found = False
            for a_dict in arr:
                if JSC_PROPERTIES in a_dict:
                    # found properties
                    arr_props = a_dict[JSC_PROPERTIES]
                    for v_k, v_v in v.items():
                        arr_props[v_k] = v_v  # add or overwrite
                    props_found = True
            if not props_found:
                arr.append(element)
        elif k == JSC_REF:
            ref_found = False
            for a_dict in arr:
                if k in a_dict and a_dict[k] == v:
                    ref_found = True
                    continue
            if not ref_found:
                arr.append(element)
        else:
            # TODO: Required might require special handling
            pass


def convert_schema(path, vschema, un_extend=True):
    import esphome.config_validation as cv

    # analyze input key, if it is not a Required or Optional, then it is an array
    output = {}

    if str(vschema) in ejs.hidden_schemas:
        if ejs.hidden_schemas[str(vschema)] == "automation":
            vschema = vschema(ejs.jschema_extractor)
            jschema = get_jschema(path, vschema, True)
            return add_definition_array_or_single_object(jschema)
        else:
            vschema = vschema(ejs.jschema_extractor)

    if un_extend:
        extended = ejs.extended_schemas.get(str(vschema))
        if extended:
            lhs = get_jschema(path, extended[0], False)
            rhs = get_jschema(path, extended[1], False)

            # check if we are not merging properties which are already in base component
            lprops = get_all_properties(lhs)
            rprops = get_all_properties(rhs)

            if all(item in lprops for item in rprops):
                return lhs
            if all(item in rprops for item in lprops):
                return rhs

            # merge
            if JSC_ALLOF in lhs and JSC_ALLOF in rhs:
                output = lhs
                for k in rhs[JSC_ALLOF]:
                    merge(output[JSC_ALLOF], k)
            elif JSC_ALLOF in lhs:
                output = lhs
                merge(output[JSC_ALLOF], rhs)
            elif JSC_ALLOF in rhs:
                output = rhs
                merge(output[JSC_ALLOF], lhs)
            else:
                output = {JSC_ALLOF: [lhs]}
                merge(output[JSC_ALLOF], rhs)

            return output

    # When schema contains all, all also has a schema which points
    # back to the containing schema

    if isinstance(vschema, MockObj):
        return output

    while hasattr(vschema, "schema") and not hasattr(vschema, "validators"):
        vschema = vschema.schema

    if hasattr(vschema, "validators"):
        output = default_schema()
        for v in vschema.validators:
            if v:
                # we should take the valid schema,
                # commonly all is used to validate a schema, and then a function which
                # is not a schema es also given, get_schema will then return a default_schema()
                val_schema = get_jschema(path, v, False)
                if is_default_schema(val_schema):
                    if not output:
                        output = val_schema
                else:
                    if is_default_schema(output):
                        output = val_schema
                    else:
                        output = {**output, **val_schema}
        return output

    if not vschema:
        return output

    if not hasattr(vschema, "keys"):
        return get_entry(path, vschema)

    key = list(vschema.keys())[0]

    # used for platformio_options in core_config
    # pylint: disable=comparison-with-callable
    if key == cv.string_strict:
        output["type"] = "object"
        return output

    props = output[JSC_PROPERTIES] = {}
    required = []

    output["type"] = ["object", "null"]
    if DUMP_COMMENTS:
        output[JSC_COMMENT] = "converted: " + path + "/" + str(vschema)

    if path == "logger-logs":
        tags = get_logger_tags()
        for k in tags:
            props[k] = {
                "enum": [
                    "NONE",
                    "ERROR",
                    "WARN",
                    "INFO",
                    "DEBUG",
                    "VERBOSE",
                    "VERY_VERBOSE",
                ]
            }

    else:
        for k in vschema:
            if str(k).startswith("<function"):
                # generate all logger tags

                # TODO handle key functions

                continue

            v = vschema[k]
            prop = {}

            if isinstance(v, vol.Schema):
                prop = get_jschema(path + "-" + str(k), v.schema)
            elif hasattr(v, "validators"):
                prop = convert_schema(path + "-" + str(k), v, False)
            else:
                prop = get_entry(path + "-" + str(k), v)

            if prop:  # Deprecated (cv.Invalid) properties not added
                props[str(k)] = prop
                # TODO: see required, sometimes completions doesn't show up because of this...
                if isinstance(k, cv.Required):
                    required.append(str(k))
                try:
                    if str(k.default) != "...":
                        default_value = k.default()
                        # Yaml validator fails if `"default": null` ends up in the json schema
                        if default_value is not None:
                            if prop["type"] == "string":
                                default_value = str(default_value)
                            prop["default"] = default_value
                except:
                    pass

    if len(required) > 0:
        output[JSC_REQUIRED] = required
    return output


def add_pin_schema():
    from esphome import pins

    add_module_schemas("PIN", pins)


def add_pin_registry():
    from esphome import pins

    pin_registry = pins.PIN_SCHEMA_REGISTRY
    assert len(pin_registry) > 0
    # Here are schemas for pcf8574, mcp23xxx and other port expanders which add
    # gpio registers
    # ESPHome validates pins schemas if it founds a key in the pin configuration.
    # This key is added to a required in jsonschema, and all options are part of a
    # oneOf section, so only one is selected. Also internal schema adds number as required.

    for mode in ("INPUT", "OUTPUT"):
        schema_name = f"PIN.GPIO_FULL_{mode}_PIN_SCHEMA"
        internal = definitions[schema_name]
        definitions[schema_name]["additionalItems"] = False
        definitions[f"PIN.{mode}_INTERNAL"] = internal
        internal[JSC_PROPERTIES]["number"] = {"type": ["number", "string"]}
        schemas = [get_ref(f"PIN.{mode}_INTERNAL")]
        schemas[0]["required"] = ["number"]
        # accept string and object, for internal shorthand pin IO:
        definitions[schema_name] = {"oneOf": schemas, "type": ["string", "object"]}

        for k, v in pin_registry.items():
            pin_jschema = get_jschema(
                f"PIN.{mode}_" + k, v[1][0 if mode == "OUTPUT" else 1]
            )
            if unref(pin_jschema):
                pin_jschema["required"] = [k]
                schemas.append(pin_jschema)


def dump_schema():
    import esphome.config_validation as cv

    from esphome import automation
    from esphome.automation import validate_potentially_and_condition
    from esphome import pins
    from esphome.core import CORE
    from esphome.helpers import write_file_if_changed
    from esphome.components import remote_base

    # The root directory of the repo
    root = Path(__file__).parent.parent

    # Fake some directory so that get_component works
    CORE.config_path = str(root)

    file_path = args.output

    schema_registry[cv.boolean] = {"type": "boolean"}

    for v in [
        cv.int_,
        cv.int_range,
        cv.positive_int,
        cv.float_,
        cv.positive_float,
        cv.positive_float,
        cv.positive_not_null_int,
        cv.negative_one_to_one_float,
        cv.port,
    ]:
        schema_registry[v] = {"type": "number"}

    for v in [
        cv.string,
        cv.string_strict,
        cv.valid_name,
        cv.hex_int,
        cv.hex_int_range,
        pins.output_pin,
        pins.input_pin,
        pins.input_pullup_pin,
        cv.float_with_unit,
        cv.subscribe_topic,
        cv.publish_topic,
        cv.mqtt_payload,
        cv.ssid,
        cv.percentage_int,
        cv.percentage,
        cv.possibly_negative_percentage,
        cv.positive_time_period,
        cv.positive_time_period_microseconds,
        cv.positive_time_period_milliseconds,
        cv.positive_time_period_minutes,
        cv.positive_time_period_seconds,
    ]:
        schema_registry[v] = {"type": "string"}

    schema_registry[validate_potentially_and_condition] = get_ref("condition_list")

    for v in [pins.gpio_input_pin_schema, pins.gpio_input_pullup_pin_schema]:
        schema_registry[v] = get_ref("PIN.GPIO_FULL_INPUT_PIN_SCHEMA")
    for v in [pins.internal_gpio_input_pin_schema, pins.input_pin]:
        schema_registry[v] = get_ref("PIN.INPUT_INTERNAL")

    for v in [pins.gpio_output_pin_schema, pins.internal_gpio_output_pin_schema]:
        schema_registry[v] = get_ref("PIN.GPIO_FULL_OUTPUT_PIN_SCHEMA")
    for v in [pins.internal_gpio_output_pin_schema, pins.output_pin]:
        schema_registry[v] = get_ref("PIN.OUTPUT_INTERNAL")

    add_module_schemas("CONFIG", cv)
    get_jschema("POLLING_COMPONENT", cv.polling_component_schema("60s"))

    add_pin_schema()

    add_module_schemas("REMOTE_BASE", remote_base)
    add_module_schemas("AUTOMATION", automation)

    load_components()
    add_registries()

    definitions["condition_list"] = {
        JSC_ONEOF: [
            {"type": "array", "items": get_ref(JSC_CONDITION)},
            get_ref(JSC_CONDITION),
        ]
    }

    output = {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "type": "object",
        "definitions": definitions,
        JSC_PROPERTIES: base_props,
    }

    add_core()
    add_buses()
    add_components()

    add_registries()  # need second pass, e.g. climate.pid.autotune
    add_pin_registry()
    solve_pending_refs()

    write_file_if_changed(file_path, json.dumps(output))
    print(f"Wrote {file_path}")


dump_schema()
