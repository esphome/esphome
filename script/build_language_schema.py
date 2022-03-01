import inspect
import json
import argparse
import os
from tokenize import Number
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


S_CONFIG_VARS = "config_vars"
S_CONFIG_SCHEMA = "CONFIG_SCHEMA"
S_COMPONENT = "component"
S_COMPONENTS = "components"
S_PLATFORMS = "platforms"
S_SCHEMA = "schema"
S_SCHEMAS = "schemas"
S_EXTENDS = "extends"
S_NAME = "name"

parser = argparse.ArgumentParser()
parser.add_argument(
    "--output-path", default=".", help="Output path", type=os.path.abspath
)

args = parser.parse_args()

DUMP_RAW = False

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
    output[module][S_SCHEMAS][name] = convert_schema(schema, f"{module}/{name}")
    raw = str(schema)
    if DUMP_RAW:
        output[module][S_SCHEMAS][name]["raw"] = raw
    known_schemas[raw] = f"{module}.{name}"


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
                output[reg_domain][reg_type][reg_entry_name] = convert_schema(
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
        config_var["type"] = "registry"
        config_var["registry"] = found_registries[str(registry)]

    # do pin registries
    pins_providers = schema_core["pins"] = []
    for pin_registry in pins.PIN_SCHEMA_REGISTRY:
        s = convert_schema(
            pins.PIN_SCHEMA_REGISTRY[pin_registry][1], f"pins/{pin_registry}"
        )
        if pin_registry not in output:
            output[pin_registry] = {}  # mcp23xxx does not create a component yet
        output[pin_registry]["pin"] = s
        pins_providers.append(pin_registry)

    do_esp8266()
    do_esp32()

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
    obj["type"] = "enum"
    obj["values"] = items


def isConvertibleSchema(schema):
    if schema is not None and isinstance(schema, (cv.Schema, cv.All)):
        return True
    if isinstance(schema, dict):
        for k in schema.keys():
            if isinstance(k, (cv.Required, cv.Optional)):
                return True
    return False


def convert_schema(schema, path):
    converted = {S_CONFIG_VARS: {}}
    convert_1(schema, converted, path)
    return converted


def convert_1(schema, converted, path):
    str_schema = str(schema)

    if str_schema in known_schemas:
        if "extends" not in converted:
            converted["extends"] = []
        converted["extends"].append(known_schemas[str_schema])
        return

    # Extended schemas are tracked when the .extend() is used in a schema
    if str_schema in ejs.extended_schemas:
        i = 0
        for s in ejs.extended_schemas[str_schema]:
            i = i + 1
            convert_1(s, converted, path + f"/ext {i}")
        return

    if isinstance(schema, cv.All):
        i = 0
        for inner in schema.validators:
            i = i + 1
            convert_1(inner, converted, path + f"/val {i}")

    if isinstance(schema, cv.Schema):
        convert_1(schema.schema, converted, path + "/all")

    elif isinstance(schema, dict):
        convert_keys(converted, schema, path)

    else:
        converted["type"] = str(type(schema))
        if DUMP_RAW:
            converted["raw"] = str(schema)

        if schema == cv.boolean:
            converted["type"] = "boolean"

        elif str(schema) in ejs.list_schemas:
            converted["is_list"] = True
            schema = ejs.list_schemas[str(schema)][0]
            converted["schema"] = convert_schema(schema, path + "/list")

        if isinstance(schema, vol.Schema):
            # test: esphome/project
            converted["type"] = "schema"
            converted["schema"] = convert_schema(schema.schema, path + "/s")

        elif schema == automation.validate_potentially_and_condition:
            converted["type"] = "registry"
            converted["registry"] = "condition"

        elif str(schema) in pin_validators:
            converted |= pin_validators[str(schema)]
            converted["type"] = "pin"

        elif str(schema) in ejs.hidden_schemas:
            schema_type = ejs.hidden_schemas[str(schema)]

            data = schema(ejs.jschema_extractor)

            # enums, e.g. esp32/variant
            if schema_type == "one_of":
                converted["type"] = "enum"
                converted["values"] = list(data)
            elif schema_type == "enum":
                converted["type"] = "enum"
                converted["values"] = list(data.keys())
            elif schema_type == "maybe":
                converted["type"] = "maybe"
                converted["schema"] = convert_schema(data, path + "/maybe")
            # esphome/on_boot
            elif schema_type == "automation":
                extra_schema = None
                converted["type"] = "trigger"
                if automation.AUTOMATION_SCHEMA == ejs.extended_schemas[str(data)][0]:
                    extra_schema = ejs.extended_schemas[str(data)][1]
                if extra_schema is not None:
                    automation_schema = convert_schema(extra_schema, path + "/extra")
                    if not (
                        len(automation_schema["config_vars"]) == 1
                        and "trigger_id" in automation_schema["config_vars"]
                    ):
                        automation_schema["config_vars"]["then"] = {"type": "trigger"}

                        converted["type"] = "trigger"
                        converted["schema"] = automation_schema
                        # some triggers can have a list of actions directly, while others needs to have some other configuration,
                        # e.g. sensor.on_value_rang, and the list of actions is only accepted under "then" property.
                        try:
                            schema({"delay": "1s"})
                        except cv.Invalid:
                            converted["has_required_var"] = True
            elif schema_type == "effects":
                converted["type"] = "registry"
                converted["registry"] = "light.effects"
                converted["filter"] = data[0]
            else:
                raise Exception("Unknown extracted schema type")

        elif str(schema) in ejs.registry_schemas:
            solve_registry.append((ejs.registry_schemas[str(schema)], converted))

        elif str(schema) in ejs.typed_schemas:
            converted["type"] = "typed"
            types = converted["types"] = {}
            for schema_key, schema_type in ejs.typed_schemas[str(schema)][0][0].items():
                types[schema_key] = convert_schema(
                    schema_type, path + "/type_" + schema_key
                )

        elif schema == cv.int_ or schema == cv.int_range:
            converted["type"] = "integer"
        else:
            converted["unknown"] = str(schema)

        converted["path"] = path


def is_overriden_key(key, converted):
    # check if the key is in any extended schema in this converted schema, i.e.
    # if we see a on_value_range in a dallas sensor, then this is overridden because
    #  it is already defined in sensor
    if S_EXTENDS not in converted:
        return False
    for s in converted[S_EXTENDS]:
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
        # convert_value(result, v, path + f"/{str(k)}")
        if S_CONFIG_VARS not in converted:
            converted["type"] = "schema"
            converted["schema"] = {S_CONFIG_VARS: {}}
            converted = converted["schema"]
            print(path)
        converted[S_CONFIG_VARS][str(k)] = result


build_schema()
