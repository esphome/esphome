from ast import Str
import json
import argparse
from lib2to3.pytree import convert
import os
from tokenize import Number
import voluptuous as vol

# NOTE: Cannot import other esphome components globally as a modification in jsonschema
# is needed before modules are loaded
import esphome.jsonschema as ejs

ejs.EnableJsonSchemaCollect = True

S_CONFIG_VARS = "config_vars"
S_COMPONENT = "component"
S_COMPONENTS = "components"
S_PLATFORMS = "platforms"
S_SCHEMA = "schema"
S_NAME = "name"

parser = argparse.ArgumentParser()
parser.add_argument(
    "--output-path", default=".", help="Output path", type=os.path.abspath
)

args = parser.parse_args()

# dynamic load of esphome modules (core modules and components)
modules = {}

# All components loaded in a dictionary, key is the component name and values are ComponentManifest
components = {}
platforms = {}

# A string, string map, key is the str(schema) and value is the name given
known_schemas = {}

core = {}


def get_component_names():
    return ["esphome", "wifi"]
    return ["sensor", "dallas", "binary_sensor", "gpio", "template"]
    from esphome.loader import CORE_COMPONENTS_PATH

    component_names = [
        d
        for d in os.listdir(CORE_COMPONENTS_PATH)
        if not d.startswith("__")
        and os.path.isdir(os.path.join(CORE_COMPONENTS_PATH, d))
    ]
    return component_names


def load_components():
    import esphome.config_validation as cv
    from esphome.config import get_component

    modules["cv"] = cv
    from esphome import automation

    modules["automation"] = automation

    for domain in get_component_names():
        components[domain] = get_component(domain)
        modules[domain] = components[domain].module


load_components()

# Import esphome after loading components (so schema is tracked)
import esphome.core as esphome_core
import esphome.config_validation as cv
from esphome.loader import get_platform, ComponentManifest
from esphome.helpers import write_file_if_changed


def write_file(name, obj):
    full_path = os.path.join(args.output_path, name + ".json")
    write_file_if_changed(full_path, json.dumps(obj))
    print(f"Wrote {full_path}")


def register_known_schema(name, schema, converted=None):
    known_schemas[str(schema)] = name
    if converted is not None:
        core["extended"][name] = converted


def build_schema():

    print("Building schema")

    # check esphome was not loaded globally (IDE auto imports)
    if len(ejs.extended_schemas) == 0:
        raise "no data collected. Did you globally import an ESPHome component?"

    core["extended"] = {}
    # Platforms
    core[S_PLATFORMS] = core_platforms = []
    # Components
    core[S_COMPONENTS] = core_components = {}
    # Core schema
    register_known_schema(
        "entity_base", cv.ENTITY_BASE_SCHEMA, convert_schema(cv.ENTITY_BASE_SCHEMA)
    )

    register_known_schema(
        "mqtt_base", cv.MQTT_COMPONENT_SCHEMA, convert_schema(cv.MQTT_COMPONENT_SCHEMA)
    )

    for domain, manifest in components.items():
        if manifest.is_platform_component:
            core_platforms.append(domain)
        elif manifest.config_schema is not None:
            core_components[domain] = {
                S_SCHEMA: {
                    S_COMPONENT: {}
                },  # empty schema as will be loaded from component file, this means component is root / hub
                "multi_conf": manifest.multi_conf,
            }
    write_file("core", core)

    # Generate platforms (e.g. sensor, binary_sensor, climate )
    for platform in core_platforms:
        platforms[platform] = {S_PLATFORMS: []}

        for domain, manifest in components.items():
            if domain == platform:
                assert manifest.is_platform_component
                schema = getattr(manifest.module, f"{platform}_SCHEMA".upper(), False)
                platforms[platform][S_SCHEMA] = {S_COMPONENT: convert_schema(schema)}
                register_known_schema(platform, schema)
            module = get_platform(platform, domain)
            if module is not None:
                platforms[platform][S_PLATFORMS].append(domain)

        write_file(platform, platforms[platform])

    # Generate components (e.g. dallas, gpio, template)
    for domain, manifest in components.items():
        if manifest.is_platform_component:
            continue
        c = {S_SCHEMA: {}}
        if manifest.config_schema:
            c[S_SCHEMA][S_COMPONENT] = convert_schema(manifest.config_schema)

        for platform in core_platforms:
            module = get_platform(platform, domain)
            if module is not None:
                c[S_SCHEMA][platform] = convert_schema(
                    module.config_schema, manifest, platform
                )

        write_file(domain, c)


class ConvertInfo:
    manifest: ComponentManifest = None
    platform: str
    platform_schema: None
    count: Number = 0


def convert_schema(schema, manifest=None, platform=None):
    converted = {S_CONFIG_VARS: {}}
    info = ConvertInfo()
    info.manifest = manifest
    info.platform = platform
    if platform is not None:
        info.platform_schema = platforms[platform]
    else:
        info.platform_schema = None

    convert_1(schema, converted, info, "")
    return converted


def convert_1(schema, converted, info: ComponentManifest, path):
    str_schema = str(schema)
    info.count = info.count + 1
    # print(f"{info.count} {path} {len(str_schema)} {str_schema[0:100]}")
    if info.count > 30:
        return

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
            convert_1(s, converted, info, f"{path}/extend {i}")
        return

    if isinstance(schema, cv.All):
        i = 0
        for inner in schema.validators:
            i = i + 1
            convert_1(inner, converted, info, f"{path}/validators {i}")

    if isinstance(schema, cv.Schema):
        convert_keys(converted, schema.schema, info)
    elif isinstance(schema, dict):
        convert_keys(converted, schema, info)


def convert_keys(converted, schema, info: ConvertInfo):

    for k, v in schema.items():
        if str(v).startswith("<function invalid"):
            continue

        if info.platform_schema is not None:
            if str(k) in info.platform_schema[S_SCHEMA][S_COMPONENT][S_CONFIG_VARS]:
                continue

        result = {}

        if isinstance(k, cv.GenerateID):
            result["key"] = "GeneratedID"
        elif isinstance(k, cv.Required):
            result["key"] = "Optional"
        elif isinstance(k, cv.Optional):
            result["key"] = "Optional"
        elif k == cv.string_strict:
            # this is when the key is any string, e.g. platformio_options
            converted["type"] = "dict"
            return
        else:
            raise "Unexpected key type"

        esphome_core.CORE.data = {
            esphome_core.KEY_CORE: {esphome_core.KEY_TARGET_PLATFORM: "esp8266"}
        }
        if str(k.default) != "...":
            default_value = k.default()
            if default_value is not None:
                result["default"] = str(default_value)
        result["type"] = str(type(v))
        result["raw"] = str(v)

        if isinstance(v, vol.Schema):
            result["schema"] = convert_schema(v.schema)

        converted[S_CONFIG_VARS][str(k)] = result


build_schema()
