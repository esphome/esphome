import inspect
import json
import argparse
import os
import voluptuous as vol


# NOTE: Cannot import other esphome components globally as a modification in jsonschema
# is needed before modules are loaded
import esphome.jsonschema as ejs

ejs.EnableJsonSchemaCollect = True

# schema format:
# Schemas are splitted in several files in json format, one for core stuff, one for each platform (sensor, binary_sensor, etc) and
# one for each component (dallas, sim800l, etc.) component can have schema for root component/hub and also for platform component,
# e.g. dallas has hub component which has pin and then has the sensor platform which has sensor name, index, etc.
# When files are loaded they are merged in a single object.
# The root format is

S_CONFIG_VAR = "config_var"
S_CONFIG_VARS = "config_vars"
S_CONFIG_SCHEMA = "CONFIG_SCHEMA"
S_COMPONENT = "component"
S_COMPONENTS = "components"
S_PLATFORMS = "platforms"
S_SCHEMA = "schema"
S_SCHEMAS = "schemas"
S_EXTENDS = "extends"
S_TYPE = "type"
S_NAME = "name"

parser = argparse.ArgumentParser()
parser.add_argument(
    "--output-path", default=".", help="Output path", type=os.path.abspath
)

args = parser.parse_args()

DUMP_RAW = False
DUMP_UNKNOWN = False
DUMP_PATH = False

# store here dynamic load of esphome components
components = {}

schema_core = {}

# output is where all is built
output = {"core": schema_core}
# The full generated output is here here
schema_full = {"components": output}

# A string, string map, key is the str(schema) and value is the schema path given
known_schemas = {}

solve_registry = []


def get_component_names():
    return [
        "esphome",
        "esp32",
        "esp8266",
        "logger",
        "ota",
        "i2c",
        "api",
        "wifi",
        "sim800l",
        "dallas",
        "sensor",
        "binary_sensor",
        "gpio",
        "template",
        "pn532",
        "pn532_i2c",
        "pcf8574",
        "light",
        "binary",
        "monochromatic",
    ]
    from esphome.loader import CORE_COMPONENTS_PATH

    component_names = ["esphome"]
    for d in os.listdir(CORE_COMPONENTS_PATH):
        if not d.startswith("__") and os.path.isdir(
            os.path.join(CORE_COMPONENTS_PATH, d)
        ):
            component_names.append(d)

    return component_names


def load_components():
    from esphome.config import get_component

    for domain in get_component_names():
        components[domain] = get_component(domain)


load_components()

# Import esphome after loading components (so schema is tracked)
# pylint: disable=wrong-import-position
import esphome.core as esphome_core
import esphome.config_validation as cv
import esphome.automation as automation
import esphome.pins as pins
from esphome.loader import get_platform, ComponentManifest
from esphome.helpers import write_file_if_changed
from esphome.util import Registry

# pylint: enable=wrong-import-position


def write_file(name, obj):
    full_path = os.path.join(args.output_path, name + ".json")
    write_file_if_changed(full_path, json.dumps(obj))
    print(f"Wrote {full_path}")


def register_known_schema(
    module, name, schema, manifest: ComponentManifest = None, platform=None
):
    if module not in output:
        output[module] = {S_SCHEMAS: {}}
    config = convert_config(schema, f"{module}/{name}")
    output[module][S_SCHEMAS][name] = config[S_SCHEMA] if S_SCHEMA in config else config
    known_schemas[str(schema)] = f"{module}.{name}"


def module_schemas(module):
    # This should yield elements in order so extended schemas are resolved properly
    # To do this we check on the source code where the symbol is seen first. Seems to work.
    try:
        module_str = inspect.getsource(module)
    except TypeError:
        # improv
        module_str = ""
    except OSError:
        # some empty __init__ files
        module_str = ""
    schemas = {}
    for m_attr_name in dir(module):
        m_attr_obj = getattr(module, m_attr_name)
        if isConvertibleSchema(m_attr_obj):
            schemas[module_str.find(m_attr_name)] = [m_attr_name, m_attr_obj]

    for pos in sorted(schemas.keys()):
        yield schemas[pos]


found_registries = {}

# Pin validators keys are the functions in pin which validate the pins
pin_validators = {}


def add_pin_validators():
    for m_attr_name in dir(pins):
        if "gpio" in m_attr_name:
            s = pin_validators[str(getattr(pins, m_attr_name))] = {}
            if "schema" in m_attr_name:
                s["schema"] = True  # else is just number
            if "internal" in m_attr_name:
                s["internal"] = True
            if "input" in m_attr_name:
                s["modes"] = ["input"]
            elif "output" in m_attr_name:
                s["modes"] = ["output"]
            else:
                s["modes"] = []
            if "pullup" in m_attr_name:
                s["modes"].append("pullup")


def add_module_registries(domain, module):
    for attr_name in dir(module):
        attr_obj = getattr(module, attr_name)
        if isinstance(attr_obj, Registry):
            if attr_obj == automation.ACTION_REGISTRY:
                reg_type = "action"
                reg_domain = "core"
                found_registries[str(attr_obj)] = reg_type
            elif attr_obj == automation.CONDITION_REGISTRY:
                reg_type = "condition"
                reg_domain = "core"
                found_registries[str(attr_obj)] = reg_type
            else:  # attr_name == "FILTER_REGISTRY":
                reg_domain = domain
                reg_type = attr_name.partition("_")[0].lower()
                found_registries[str(attr_obj)] = f"{domain}.{reg_type}"

            for name in attr_obj.keys():
                if "." not in name:
                    reg_entry_name = name
                else:
                    parts = name.partition(".")
                    reg_domain = parts[0]
                    reg_entry_name = parts[2]

                if reg_domain not in output:
                    output[reg_domain] = {}
                if reg_type not in output[reg_domain]:
                    output[reg_domain][reg_type] = {}
                output[reg_domain][reg_type][reg_entry_name] = convert_config(
                    attr_obj[name].schema, f"{reg_domain}/{reg_type}/{reg_entry_name}"
                )

                print(f"{domain} - {attr_name} - {name}")


def do_esp32():
    import esphome.components.esp32.boards as esp32_boards

    setEnum(
        output["esp32"]["schemas"]["CONFIG_SCHEMA"]["config_vars"]["board"],
        list(esp32_boards.BOARD_TO_VARIANT.keys()),
    )


def do_esp8266():
    import esphome.components.esp8266.boards as esp8266_boards

    setEnum(
        output["esp8266"]["schemas"]["CONFIG_SCHEMA"]["config_vars"]["board"],
        list(esp8266_boards.ESP8266_BOARD_PINS.keys()),
    )


def shrink():
    """Shrink the extending schemas which has just an end type, e.g. at this point
    ota / port is type schema with extended pointing to core.port, this should instead be
    type number. core.port is number"""

    for k, v in output.items():
        print("Simplifying " + k)
        if S_SCHEMAS in v:
            for kk, vv in v[S_SCHEMAS].items():
                if S_TYPE in vv:
                    pass
                    # print("    type")
                elif S_CONFIG_VARS in vv or S_EXTENDS in vv:
                    # print("    schema")
                    shrink_schema(vv, "      ")
                else:
                    # print("    skip")
                    pass


def shrink_schema(schema, depth):
    assert S_CONFIG_VARS not in schema or len(schema[S_CONFIG_VARS]) > 0
    if S_CONFIG_VARS not in schema and len(schema.get(S_EXTENDS, [])) == 1:
        #       print(depth + " Candidate ")
        return get_simple_type(schema.get(S_EXTENDS)[0])

    for k, v in schema.get(S_CONFIG_VARS, {}).items():
        if v.get(S_TYPE) == "schema":
            #            print(depth + "  " + k)
            simple = shrink_schema(v.get(S_SCHEMA), depth + "  ")
            if simple is not None:
                v[S_TYPE] = simple
                v.pop(S_SCHEMA)
                print(depth + k + " patch with " + simple)


def get_simple_type(ref):
    parts = str(ref).partition(".")
    data = output[parts[0]][S_SCHEMAS][parts[2]]
    if S_CONFIG_VARS in data:
        return None
    if S_TYPE in data:
        return data[S_TYPE]
    if S_EXTENDS in data and len(data[S_EXTENDS]) == 1:
        return get_simple_type(data[S_EXTENDS][0])


# print(ref)


def build_schema():
    print("Building schema")

    # check esphome was not loaded globally (IDE auto imports)
    if len(ejs.extended_schemas) == 0:
        raise Exception(
            "no data collected. Did you globally import an ESPHome component?"
        )

    # Core schema
    schema_core[S_SCHEMAS] = {}
    for name, schema in module_schemas(cv):
        register_known_schema("core", name, schema)

    platforms = []
    schema_core[S_PLATFORMS] = platforms
    core_components = []
    schema_core[S_COMPONENTS] = core_components

    add_pin_validators()

    # Load a preview of each component
    for domain, manifest in components.items():
        if manifest.is_platform_component:
            # e.g. sensor, binary sensor, add S_COMPONENTS
            # note: S_COMPONENTS is not filled until loaded, e.g.
            # if lock: is not used, then we don't need to know about their
            # platforms yet.
            output[domain] = {S_COMPONENTS: [], S_SCHEMAS: {}}
            platforms.append(domain)
        elif manifest.config_schema is not None:
            # e.g. dallas
            output[domain] = {S_SCHEMAS: {S_CONFIG_SCHEMA: {}}}

    # Generate platforms (e.g. sensor, binary_sensor, climate )
    for domain in platforms:
        c = components[domain]
        for name, schema in module_schemas(c.module):
            register_known_schema(domain, name, schema)

    # Generate components
    for domain, manifest in components.items():
        if domain in platforms:
            continue
        if manifest.config_schema is not None:
            core_components.append(domain)
        for name, schema in module_schemas(manifest.module):
            register_known_schema(domain, name, schema)
        for platform in platforms:
            platform_manifest = get_platform(domain=platform, platform=domain)
            if platform_manifest is not None:
                output[platform][S_COMPONENTS].append(domain)
                for name, schema in module_schemas(platform_manifest.module):
                    register_known_schema(f"{domain}.{platform}", name, schema)

    # Do registries
    add_module_registries("core", automation)
    for domain, manifest in components.items():
        add_module_registries(domain, manifest.module)

    # update props pointing to registries
    for reg_config_var in solve_registry:
        (registry, config_var) = reg_config_var
        config_var[S_TYPE] = "registry"
        config_var["registry"] = found_registries[str(registry)]

    # do pin registries
    pins_providers = schema_core["pins"] = []
    for pin_registry in pins.PIN_SCHEMA_REGISTRY:
        s = convert_config(
            pins.PIN_SCHEMA_REGISTRY[pin_registry][1], f"pins/{pin_registry}"
        )
        if pin_registry not in output:
            output[pin_registry] = {}  # mcp23xxx does not create a component yet
        output[pin_registry]["pin"] = s
        pins_providers.append(pin_registry)

    do_esp8266()
    do_esp32()

    shrink()

    # aggregate components, so all component info is in same file, otherwise we have dallas.json, dallas.sensor.json, etc.
    data = {}
    for component, component_schemas in output.items():
        if "." in component:
            key = component.partition(".")[0]
            if key not in data:
                data[key] = {}
            data[key][component] = component_schemas
        else:
            data[component] = {component: component_schemas}

    # bundle core inside esphome
    data["esphome"]["core"] = data.pop("core")["core"]

    for c, s in data.items():
        write_file(c, s)


def setEnum(obj, items):
    obj[S_TYPE] = "enum"
    obj["values"] = items


def isConvertibleSchema(schema):
    if schema is not None and isinstance(schema, (cv.Schema, cv.All)):
        return True
    if isinstance(schema, dict):
        for k in schema.keys():
            if isinstance(k, (cv.Required, cv.Optional)):
                return True
    return False


def convert_config(schema, path):
    converted = {}
    convert_1(schema, converted, path)
    return converted


def convert_1(schema, config_var, path):
    """config_var can be a config_var or a schema: both are dicts
    config_var has a S_TYPE property, if this is S_SCHEMA, then it has a S_SCHEMA property
    schema does not have a type property, schema can have optionally both S_CONFIG_VARS and S_EXTENDS
    """
    str_schema = str(schema)

    if str_schema in known_schemas:
        assert S_CONFIG_VARS not in config_var
        assert S_EXTENDS not in config_var
        if not S_TYPE in config_var:
            config_var[S_TYPE] = S_SCHEMA
        assert config_var[S_TYPE] == S_SCHEMA

        if S_SCHEMA not in config_var:
            config_var[S_SCHEMA] = {}
        if S_EXTENDS not in config_var[S_SCHEMA]:
            config_var[S_SCHEMA][S_EXTENDS] = [known_schemas[str_schema]]
        else:
            config_var[S_SCHEMA][S_EXTENDS].append(known_schemas[str_schema])
        return

    # Extended schemas are tracked when the .extend() is used in a schema
    if str_schema in ejs.extended_schemas:
        extended = ejs.extended_schemas.get(str_schema)

        # The midea actions are extending an empty schema (resulted in the templatize not templatizing anything)
        # this causes a recursion in that this extended looks the same in extended schema as the extended[1]
        if str_schema == str(extended[1]):
            assert path == "midea.climate/MIDEA_ACTION_BASE_SCHEMA"
            return

        assert len(extended) == 2
        convert_1(extended[0], config_var, path + "/extL")
        convert_1(extended[1], config_var, path + "/extR")
        return

    if isinstance(schema, cv.All):
        i = 0
        for inner in schema.validators:
            i = i + 1
            convert_1(inner, config_var, path + f"/val {i}")
        return

    if hasattr(schema, "validators"):
        i = 0
        for inner in schema.validators:
            i = i + 1
            convert_1(inner, config_var, path + f"/val {i}")

    if isinstance(schema, cv.Schema):
        convert_1(schema.schema, config_var, path + "/all")
        return

    if isinstance(schema, dict):
        convert_keys(config_var, schema, path)
        return

    if str_schema in ejs.list_schemas:
        config_var["is_list"] = True
        items_schema = ejs.list_schemas[str_schema][0]
        convert_1(items_schema, config_var, path + "/list")
        return

    if DUMP_RAW:
        config_var["raw"] = str_schema

    # pylint: disable=comparison-with-callable
    if schema == cv.boolean:
        config_var[S_TYPE] = "boolean"
    elif schema == automation.validate_potentially_and_condition:
        config_var[S_TYPE] = "registry"
        config_var["registry"] = "condition"
    elif schema == cv.int_ or schema == cv.int_range:
        config_var[S_TYPE] = "integer"
    elif schema == cv.string or schema == cv.string_strict or schema == cv.valid_name:
        config_var[S_TYPE] = "string"

    elif isinstance(schema, vol.Schema):
        # test: esphome/project
        config_var[S_TYPE] = "schema"
        config_var["schema"] = convert_config(schema.schema, path + "/s")["schema"]

    elif str_schema in pin_validators:
        config_var |= pin_validators[str_schema]
        config_var[S_TYPE] = "pin"

    elif str_schema in ejs.hidden_schemas:
        schema_type = ejs.hidden_schemas[str_schema]

        data = schema(ejs.jschema_extractor)

        # enums, e.g. esp32/variant
        if schema_type == "one_of":
            config_var[S_TYPE] = "enum"
            config_var["values"] = list(data)
        elif schema_type == "enum":
            config_var[S_TYPE] = "enum"
            config_var["values"] = list(data.keys())
        elif schema_type == "maybe":
            config_var[S_TYPE] = "maybe"
            config_var["schema"] = convert_config(data, path + "/maybe")["schema"]
        # esphome/on_boot
        elif schema_type == "automation":
            extra_schema = None
            config_var[S_TYPE] = "trigger"
            if automation.AUTOMATION_SCHEMA == ejs.extended_schemas[str(data)][0]:
                extra_schema = ejs.extended_schemas[str(data)][1]
            if extra_schema is not None:
                config = convert_config(extra_schema, path + "/extra")
                if "schema" in config:
                    automation_schema = config["schema"]
                    if not (
                        len(automation_schema["config_vars"]) == 1
                        and "trigger_id" in automation_schema["config_vars"]
                    ):
                        automation_schema["config_vars"]["then"] = {S_TYPE: "trigger"}
                        automation_schema["config_vars"].pop("trigger_id")

                        config_var[S_TYPE] = "trigger"
                        config_var["schema"] = automation_schema
                        # some triggers can have a list of actions directly, while others needs to have some other configuration,
                        # e.g. sensor.on_value_rang, and the list of actions is only accepted under "then" property.
                        try:
                            schema({"delay": "1s"})
                        except cv.Invalid:
                            config_var["has_required_var"] = True
                else:
                    print("figure out " + path)
        elif schema_type == "effects":
            config_var[S_TYPE] = "registry"
            config_var["registry"] = "light.effects"
            config_var["filter"] = data[0]
        elif schema_type == "templatable":
            config_var["templatable"] = True
            convert_1(data, config_var, path + "/templat")
        else:
            raise Exception("Unknown extracted schema type")

    elif str_schema in ejs.registry_schemas:
        solve_registry.append((ejs.registry_schemas[str_schema], config_var))

    elif str_schema in ejs.typed_schemas:
        config_var[S_TYPE] = "typed"
        types = config_var["types"] = {}
        for schema_key, schema_type in ejs.typed_schemas[str_schema][0][0].items():
            config = convert_config(schema_type, path + "/type_" + schema_key)
            types[schema_key] = config["schema"]

    elif DUMP_UNKNOWN:
        if S_TYPE not in config_var:
            config_var["unknown"] = str_schema

    if DUMP_PATH:
        config_var["path"] = path


def is_overriden_key(key, converted):
    # check if the key is in any extended schema in this converted schema, i.e.
    # if we see a on_value_range in a dallas sensor, then this is overridden because
    #  it is already defined in sensor
    assert S_CONFIG_VARS not in converted and S_EXTENDS not in converted
    config = converted.get(S_SCHEMA, {})

    if S_EXTENDS not in config:
        return False
    for s in config[S_EXTENDS]:
        p = s.partition(".")
        s1 = (
            output.get(p[0], {}).get(S_SCHEMAS, {}).get(p[2], {}).get(S_CONFIG_VARS, {})
        )
        if key in s1:
            return True
    return False


def convert_keys(converted, schema, path):
    for k, v in schema.items():
        # deprecated stuff
        if str(v).startswith("<function invalid"):
            continue

        if is_overriden_key(k, converted):
            continue

        result = {}

        if isinstance(k, cv.GenerateID):
            result["key"] = "GeneratedID"
        elif isinstance(k, cv.Required):
            result["key"] = "Required"
        elif (
            isinstance(k, cv.Optional)
            or isinstance(k, cv.Inclusive)
            or isinstance(k, cv.Exclusive)
        ):
            result["key"] = "Optional"
        else:
            converted["key"] = "String"
            converted["key_dump"] = str(k)

        esphome_core.CORE.data = {
            esphome_core.KEY_CORE: {esphome_core.KEY_TARGET_PLATFORM: "esp8266"}
        }
        if hasattr(k, "default") and str(k.default) != "...":
            default_value = k.default()
            if default_value is not None:
                result["default"] = str(default_value)

        # Do value
        convert_1(v, result, path + f"/{str(k)}")
        if "schema" not in converted:
            converted[S_TYPE] = "schema"
            converted["schema"] = {S_CONFIG_VARS: {}}
        if S_CONFIG_VARS not in converted["schema"]:
            converted["schema"][S_CONFIG_VARS] = {}
        converted["schema"][S_CONFIG_VARS][str(k)] = result


build_schema()
