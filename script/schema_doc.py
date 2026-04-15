#!/usr/bin/env python3
import argparse
import json
import os
import re
import unicodedata
from pathlib import Path
from pprint import pprint
from inspect import getmembers
from types import FunctionType

# cspell:ignore Clockless fastled dfrobot templatable

DOC_CONFIGURATION_VARIABLES = "Configuration variables:"
DOC_CONFIGURATION_OPTIONS = "Configuration options:"
DOC_OVER_SPI = "Over SPI"
DOC_OVER_I2C = "Over I²C"

JSON_CONFIG_VARS = "config_vars"
JSON_EXTENDS = "extends"
JSON_DOCS = "docs"
JSON_KEY = "key"
JSON_TEMPLATABLE = "templatable"
JSON_CV_TYPE = "type"
JSON_CV_TYPE_SCHEMA = "schema"
JSON_ACTION = "action"

DOCS_ROOT = Path(".") / "src" / "content" / "docs"


# (registry_json_key, title_suffix_or_None)
# None suffix = backtick title pattern: `name`
COMPONENT_REGISTRIES = {
    "light": [("effects", " Effect")],
    "binary_sensor": [("filter", None)],
    "text_sensor": [("filter", None)],
    "sensor": [("filter", " Filter")],  # standalone files only
}

# Display names that don't derive cleanly to their registry key via title_to_registry_key()
# key: (component, registry_json_key, display_name_after_stripping_suffix)
REGISTRY_KEY_ODDITIES = {
    ("light", "effects", "Automation Light"): "automation",
    ("light", "effects", "E1.31"): "e131",
}

args = None


def is_configuration_variables_title_alike(title):
    REGEX_CONFIGURATION_VARIABLES_TITLE = r"^#*\s?Configuration (variables|options):?$"

    return re.search(REGEX_CONFIGURATION_VARIABLES_TITLE, title, re.IGNORECASE)


def slugify(text: str) -> str:
    # Normalize Unicode to ASCII (e.g., é → e)
    text = unicodedata.normalize("NFKD", text)
    text = text.encode("ascii", "ignore").decode("ascii")
    # Lowercase
    text = text.lower()
    # Remove non-word characters (keep alphanumeric, underscores, whitespace, hyphens)
    text = re.sub(r"[^\w\s-]", "", text)
    # Replace whitespace and hyphens with single hyphen
    text = re.sub(r"[-\s]+", "-", text)
    # Trim hyphens from ends
    text = text.strip("-")
    return text


class SeeAlso:
    title: str = None
    file: Path = None
    doc_slug_title: str = None

    def reset_doc(self, md_file: Path):
        if "title" in md_docs[md_file]:
            self.doc_slug_title = None
            self.file = md_file
            self.title = md_docs[md_file]["title"]
        else:
            assert "filter" == md_file.parent.stem
            # doc title should be valid and file should not change

    def set_title_slug(self, title):
        # TODO: if setting same title, the slug actually gets appended -1, -2 etc.
        self.doc_slug_title = f"#{slugify(title)}"

    def set_title(self, title):
        self.set_title_slug(title)

    def md(self):
        relative = self.file.relative_to(DOCS_ROOT)
        url_path = "/" + "/".join(relative.parts[:-1])
        if self.file.stem != "index":
            url_path += f"/{self.file.stem}"
        if self.doc_slug_title:
            url_path += self.doc_slug_title

        return f"*See also: [{self.title}]({args.deploy_url}{url_path})*"


see_also = SeeAlso()


class Stats:
    core_docs = 0
    core_platform_docs = 0
    platform_docs = 0
    props = 0
    props_created = 0
    props_skipped = 0
    props_refined = 0
    enum_docs = 0
    action_docs = 0
    condition_docs = 0
    registry_docs = 0
    changed_optionality = 0


stats = Stats()

md_docs = {}
json_docs = {}
reverse_extends = {}  # id(parent_inner_schema) -> [child_inner_schema, ...]


def unquote(s: str) -> str:
    return re.sub(r"""^(['"])(.*)\1$""", r"\2", s)


def md_parse_frontmatter(md_file, lines):
    if lines[0] == "---":
        index = 1
        while lines[index] != "---":
            if lines[index].startswith("title: "):
                md_docs[md_file]["title"] = unquote(
                    lines[index][len("title:") :].strip()
                )
            index += 1
        return index + 1
    return 0


def open_file_lines(file):
    if os.path.exists(file):
        with open(file, "r", encoding="utf-8-sig") as file_f:
            lines = file_f.read().split("\n")
            return lines
    else:
        print(f"Error: File {file} not found")


def mrkdwn_lines(md_file):
    lines = md_docs.get(md_file, {}).get("lines")
    if lines:
        return lines

    if (lines := open_file_lines(md_file)) is not None:
        # cache into md_docs dict
        md_docs[md_file] = {"lines": lines}
        return lines


def md_get_paragraph(lines, index):
    # skip
    while (
        not lines[index].strip()
        or (  # whitespace
            lines[index].strip().startswith("{{")
            and lines[index].strip().endswith("}}")  # legacy anchors
        )
        or lines[index].strip().startswith('<span id="')  # anchors
        or (is_title(lines[index]))  # titles
    ):
        index += 1
        if index >= len(lines):
            return index, None
    paragraph = ""
    # get lines
    if lines[index].startswith("```"):  # got a code block, return None
        return index, None

    while lines[index].strip():
        paragraph = paragraph + lines[index] + " "
        index += 1
    return index, paragraph.strip()


def md_get_next_title(md_file, lines, index):
    while True:
        if index >= len(lines):
            return index, None
        line = lines[index]
        if is_configuration_variables_title_alike(line):
            if line.startswith("#"):
                see_also.set_title_slug(line)
            elif args.debug_level > 6:
                print(
                    f"{md_file}:{index + 1} {DOC_CONFIGURATION_VARIABLES} title is not # marked. Cannot generate slug link"
                )
            return index + 1, DOC_CONFIGURATION_VARIABLES
        if is_title(line):
            see_also.set_title(line)
            return index + 1, line.replace("#", "").strip()
        index += 1


def md_get_next_config(lines, index):
    # returns a - item from a list
    ret = None
    indent = 0
    in_code_block = False
    while True:
        if index >= len(lines):
            return index, None, indent
        line = lines[index]

        # skip code blocks inside properties (and complain??)
        if line.startswith("```"):
            in_code_block = not in_code_block
            index += 1
            continue
        if in_code_block:
            index += 1
            continue

        if is_title(line):
            if ret:
                return index, ret, indent
            return index, None, indent

        line = lines[index].strip()

        if line.startswith("- "):
            if ret:
                return index, ret, indent
            ret = line[2:].strip()
            indent = lines[index].find("-")
        elif ret and line:
            line_indent = len(lines[index]) - len(line)
            if line_indent == indent + 2:
                ret += " " + line
            else:
                return index, ret, indent
        index += 1


def json_get(name):
    if name == "core":
        name = "esphome"

    json_doc = json_docs.get(name)
    if json_doc:
        return json_doc

    json_file_name = os.path.join(args.read_schema_dir, name + ".json")
    if os.path.exists(json_file_name):
        if args.debug_level > 12:
            print(f"Loading {json_file_name}")
        with open(json_file_name, "r", encoding="utf-8-sig") as f:
            json_docs[name] = json_doc = json.loads(f.read())
            return json_doc
    else:
        print(f"Error: File {json_file_name} not found")
    return


def json_save():
    for name, content in json_docs.items():
        json_file_name = os.path.join(args.schema_dir, name + ".json")
        with open(json_file_name, "w", encoding="utf-8") as f:
            f.write(json.dumps(content, indent=2))


def make_doc_with_see_also(md_file, index, docs):
    docs = convert_links(md_file, index, docs)
    return f"{docs}\n\n{see_also.md()}"


def process_component(md_file, lines, index, name):
    # This adds the doc to the esphome.json file / "components"
    esphome_json = json_get("esphome")
    core = esphome_json["core"]
    if name not in core["components"]:
        return index, False
    index, docs = md_get_paragraph(lines, index)
    if JSON_DOCS not in core["components"][name]:
        core["components"][name][JSON_DOCS] = make_doc_with_see_also(
            md_file, index, docs
        )
        stats.core_docs += 1
    return index, True


def process_platform_component(md_file, lines, index, platform, name):
    # This adds the doc to the platform file / "components", e.g. sensor.json
    platform_json = json_get(platform)
    index, docs = md_get_paragraph(lines, index)
    if name in platform_json[platform]["components"]:
        platform_json[platform]["components"][name][JSON_DOCS] = make_doc_with_see_also(
            md_file, index, docs
        )
        stats.platform_docs += 1
        return index, True
    else:
        return index, False


def is_platform(name):
    return name in json_get("esphome")["core"]["platforms"]


def get_platform_from_title(title, config_component=None):
    title = title.lower().replace("`", "")
    if config_component and title.startswith(config_component.lower()):
        title = title[len(config_component) + 1 :]
    name = title.replace(" ", "_")
    if is_platform(name):
        return name
    return None


REGEX_PROP = r"^\*\*(\w+)\*\*(?: \((.*?)\))?: (.*)"  # **<group1>** (<group2>): <group3> ## group2 optional
REGEX_ENUM1 = r"^`([^`]*)`(?:(?: -|:) (.*)|\s\((.*)\))?"
REGEX_ENUM2 = r"^\*\*([^\*]*)\*\*(?:(?: -|:) (.*)|\s\((.*)\))?"
REGEX_PROP_TITLE = r"^#+ `([^`]+)`(.*)"


def find_schema_prop(schema, prop_name):
    if JSON_CONFIG_VARS in schema:
        matched_config = schema[JSON_CONFIG_VARS].get(prop_name)
        if matched_config:
            return matched_config
    for extended in schema.get(JSON_EXTENDS, []):
        parts = extended.split(".")
        extended_json = json_get(parts[0])
        if len(parts) == 3:
            extended = (
                extended_json.get(f"{parts[0]}.{parts[1]}", {})
                .get("schemas", {})
                .get(parts[2], {})
            )
        else:
            extended = (
                extended_json.get(parts[0], {}).get("schemas", {}).get(parts[1], {})
            )
        if not extended:
            print(f"Cannot find extended schema: {'.'.join(parts)}")
        if extended.get(JSON_CV_TYPE) == JSON_CV_TYPE_SCHEMA:
            matched_config = find_schema_prop(extended["schema"], prop_name)
            if matched_config:
                return matched_config
    return None


def resolve_extends_ref(ref):
    """Resolve an extends reference string to its inner schema dict."""
    parts = ref.split(".")
    ref_json = json_get(parts[0])
    if not ref_json:
        return None
    if len(parts) == 3:
        schema_def = (
            ref_json.get(f"{parts[0]}.{parts[1]}", {})
            .get("schemas", {})
            .get(parts[2], {})
        )
    else:
        schema_def = ref_json.get(parts[0], {}).get("schemas", {}).get(parts[1], {})
    if schema_def.get(JSON_CV_TYPE) == JSON_CV_TYPE_SCHEMA and "schema" in schema_def:
        return schema_def["schema"]
    return None


def fill_reverse_extends():
    """Build reverse_extends map by scanning all loaded JSON schemas."""
    for name, json_doc in json_docs.items():
        for top_key in json_doc:
            schemas = json_doc.get(top_key, {}).get("schemas", {})
            for schema_name, schema_def in schemas.items():
                if (
                    schema_def.get(JSON_CV_TYPE) == JSON_CV_TYPE_SCHEMA
                    and "schema" in schema_def
                ):
                    inner = schema_def["schema"]
                    for ext in inner.get(JSON_EXTENDS, []):
                        parent = resolve_extends_ref(ext)
                        if parent is not None:
                            reverse_extends.setdefault(id(parent), []).append(inner)


def find_schema_props_in_children(schema, prop_name):
    """Find prop_name in schemas that extend the given schema (directly or transitively).
    Returns a list of matched config dicts."""
    results = []
    queue = [schema]
    visited = set()

    while queue:
        current = queue.pop(0)
        cid = id(current)
        if cid in visited:
            continue
        visited.add(cid)

        for child in reverse_extends.get(cid, []):
            cv = child.get(JSON_CONFIG_VARS, {})
            if prop_name in cv:
                results.append(cv[prop_name])
            queue.append(child)

    return results


def convert_links(md_file, index, docs):
    if docs is None:
        return None

    REGEX_LINK = r"\[([^\]]*)\]\(([^\)]*)\)"

    def replacer(match):
        title = match.group(1)
        url = match.group(2)

        if url.startswith("http://") or url.startswith("https://"):
            return match.group(0)  # external — leave as-is

        if url.startswith("/"):
            # Absolute site path — prepend deploy_url, no lookup needed
            return f"[{title}]({args.deploy_url}{url})"

        if url.startswith("#"):
            # Same-page anchor — resolve to this file's absolute URL
            anchor = url[1:]
            relative = md_file.relative_to(DOCS_ROOT)
            url_path = "/" + "/".join(relative.parts[:-1])
            if md_file.stem != "index":
                url_path += f"/{md_file.stem}"
            return f"[{title}]({args.deploy_url}{url_path}#{anchor})"

        # Other relative links — leave as-is
        return match.group(0)

    return re.sub(REGEX_LINK, replacer, docs)


def is_templatable_type(type_part):
    return re.search(r"\[templatable\]", type_part) is not None


def title_to_registry_key(name):
    """Convert a display name to a registry key (lowercase, special chars → underscores)."""
    return re.sub(r"[^a-zA-Z0-9]+", "_", name.lower()).strip("_")


def find_registry_entry(title, config_component):
    """Check if title matches a registry entry for config_component.
    Returns (registry_json_key, entry_dict) or (None, None)."""
    for registry_key, suffix in COMPONENT_REGISTRIES.get(config_component, []):
        json_config = json_get(config_component)
        if not json_config:
            continue
        registry = json_config.get(config_component, {}).get(registry_key, {})

        if suffix is not None:
            if not title.endswith(suffix):
                continue
            name = title[: -len(suffix)].strip()
            key = REGISTRY_KEY_ODDITIES.get(
                (config_component, registry_key, name)
            ) or title_to_registry_key(name)
            entry = registry.get(key)
            if entry is not None:
                return registry_key, entry
        else:
            # Backtick style: `name`
            m = re.match(r"^`(.+)`$", title)
            if m:
                key = m.group(1)
                entry = registry.get(key)
                if entry is not None:
                    return registry_key, entry

    return None, None


def set_schema_doc(md_file, index, schema, prop_name, prop_types, doc):
    matched_config = find_schema_prop(schema, prop_name)

    if not matched_config:
        # Check if an entry from this schema has prop
        children = find_schema_props_in_children(schema, prop_name)
        if not children:
            # This prop not found either up or down the prop tree
            if args.debug_level > 8:
                print(f"{md_file}:{index}: prop {prop_name} not matched in schema")

            return None

        # document here
        matched_config = schema.setdefault(JSON_CONFIG_VARS, {}).setdefault(
            prop_name, {}
        )
        stats.props_created += 1

    converted_doc = make_doc_with_see_also(md_file, index, doc)
    if prop_types:
        type_parts = [part.strip() for part in prop_types.split(",")]
        optionality = type_parts[0].replace("*", "")
        config_optionality = matched_config.get(JSON_KEY, "")
        if (
            prop_name != "id"
            and config_optionality != "GeneratedID"
            and optionality.casefold() != config_optionality.casefold()
        ):
            # The SSOT will be docs, retrieving precise optionality by reflecting into esphome schema
            # is not accurate
            stats.changed_optionality += 1
            matched_config[JSON_KEY] = optionality.capitalize()
            if args.debug_level > 5:
                print(
                    f"{md_file}:{index} {prop_name} Key {config_optionality} in ESPHome does not match {optionality} in docs"
                )

        templatable = any(is_templatable_type(p) for p in type_parts[1:])
        config_templatable = matched_config.get(JSON_TEMPLATABLE, False)
        if templatable != config_templatable and args.debug_level > 5:
            print(
                f"{md_file}:{index} {prop_name} Templatable {config_templatable} in ESPHome does not match {templatable} in docs"
            )

        # Document with type information, unless the type just says templatable
        if len(type_parts) > 1 and not is_templatable_type(type_parts[1]):
            prop_type = convert_links(md_file, index, type_parts[1])
            converted_doc = f"**{prop_type}**: {converted_doc}"

    if JSON_DOCS in matched_config and matched_config[JSON_DOCS] == converted_doc:
        # skip re documenting

        stats.props_skipped += 1
        return matched_config

    is_extended_schema = matched_config != schema.get(JSON_CONFIG_VARS, {}).get(
        prop_name
    )

    if JSON_DOCS in matched_config and is_extended_schema:
        # override docs in extended schema here
        new_docs_schema = schema.setdefault(JSON_CONFIG_VARS, {}).setdefault(
            prop_name, {}
        )
        if JSON_KEY in matched_config:
            new_docs_schema.setdefault(JSON_KEY, matched_config[JSON_KEY])
        matched_config = new_docs_schema  # document in upper level
        stats.props_refined += 1
    else:
        stats.props += 1

    matched_config[JSON_DOCS] = converted_doc

    return matched_config


def md_skip_level(lines, index):
    line = lines[index]
    indent = len(line) - len(line.strip())
    while index + 1 < len(lines):
        index += 1
        line = lines[index]
        if indent < len(line) - len(line.strip()):
            return index
    return index + 1


def is_title(title):
    return title.startswith("#")


def is_break_title(title):
    if is_title(title):
        name = title.split(" ")[-1].lower()
        if get_platform_from_title(name):
            return True
        if name in ["action", "condition", "component"]:
            return True
        # Bare backtick heading (### `name`) — registry entry like a filter or effect.
        # Nothing after the closing backtick, so it's not a property sub-heading.
        if re.match(r"^#+\s+`[^`]+`\s*$", title):
            return True
    return False


def process_schema(
    md_file,
    lines,
    index,
    schema,
    indent,
    parent_schema,
    typed_var=None,
    typed_value=None,
):
    matched_config = None
    while True:
        if index >= len(lines):
            return index
        if is_title(lines[index]):
            if is_break_title(lines[index]):
                return index
            else:
                index += 1
        prev_index = index
        index, item_config, item_indent = md_get_next_config(lines, index)
        if index >= len(lines):
            return index

        if not item_config:
            search = re.search(REGEX_PROP_TITLE, lines[index], re.IGNORECASE)
            if search:
                prop_name = search.group(1)
                matched_config = find_schema_prop(schema, prop_name)
                if matched_config:
                    if args.debug_level > 6:
                        print(
                            f"{md_file}:{index} {lines[index]} : matched title for prop {prop_name} "
                        )
                    index = process_config(
                        md_file, lines, index + 1, matched_config, 0, schema
                    )
                    continue
                elif parent_schema:
                    matched_config = find_schema_prop(parent_schema, prop_name)
                    if matched_config:
                        return index

        if item_indent < indent:
            return prev_index
        if item_indent > indent:
            if not matched_config:
                if args.debug_level > 6:
                    print(
                        f"{md_file}:{index} {lines[index]} an indentation increase for unknown"
                    )
                next_index = md_skip_level(lines, index)
                continue
            if matched_config.get(JSON_CV_TYPE, []) not in ["enum", "schema"]:
                if args.debug_level > 6:
                    print(
                        f"{md_file}:{index} {lines[index]} : an indentation increase for a {matched_config.get(JSON_CV_TYPE, 'unknown')}"
                    )
            next_index = process_config(
                md_file, lines, prev_index, matched_config, item_indent, schema
            )
            if next_index == prev_index:
                # no progress
                next_index = index  # skip ahead
            index = next_index
            continue
        if not item_config:
            continue
        search = re.search(REGEX_PROP, item_config, re.IGNORECASE)
        if search:
            prop_name = search.group(1)
            if args.debug_level > 10:
                print(f"{md_file}:{index}: prop {prop_name}")

            if typed_var and typed_var.get("typed_key") == prop_name:
                typed_var["docs"] = search.group(3)
            else:
                matched_config = set_schema_doc(
                    md_file, index, schema, prop_name, search.group(2), search.group(3)
                )


def process_config(md_file, lines, index, config_var, indent=0, parent_schema=None):
    while True:
        if index >= len(lines):
            return index
        if is_break_title(lines[index]):
            return index
        item_type = config_var.get(JSON_CV_TYPE)
        if item_type in ["schema", "trigger"] and JSON_CV_TYPE_SCHEMA in config_var:
            schema = config_var[JSON_CV_TYPE_SCHEMA]
            return process_schema(md_file, lines, index, schema, indent, parent_schema)

        elif item_type == "typed":
            for typed in config_var["types"]:
                process_schema(
                    md_file,
                    lines,
                    index,
                    config_var["types"][typed],
                    indent,
                    None,
                    typed_var=config_var,
                    typed_value=typed,
                )

            return md_skip_level(lines, index + 1)
        elif item_type == "enum":
            prev_index = index
            index, item_config, item_indent = md_get_next_config(lines, index)
            if not item_config:
                return index
            if item_indent < indent:
                return prev_index
            search = re.search(REGEX_ENUM1, item_config, re.IGNORECASE)
            if search:
                enum_value = search.group(1)
                enum_desc = search.group(2) or search.group(3)
                values = config_var.get("values", {})
                if enum_value in values:
                    values[enum_value] = values.get(enum_value) or {}
                    values[enum_value][JSON_DOCS] = convert_links(
                        md_file, index, enum_desc
                    )
                    stats.enum_docs += 1
            else:
                search = re.search(REGEX_ENUM2, item_config, re.IGNORECASE)
                if search:
                    enum_value = search.group(1)
                    enum_desc = search.group(2) or search.group(3)
                    values = config_var.get("values", {})
                    if enum_value in values:
                        values[enum_value] = values.get(enum_value) or {}
                        values[enum_value][JSON_DOCS] = convert_links(
                            md_file, index, enum_desc
                        )
                        stats.enum_docs += 1
                else:
                    print(f"{md_file}:{index} Cannot get enum expected here")

        elif item_type is None or item_type == "string":
            # consume this level
            prev_index = index
            index, item_config, item_indent = md_get_next_config(lines, index)
            if not item_config or item_indent != indent:
                return prev_index if item_config else index
        else:
            return index


def oddities_doc_not_specific_component(folder, file):
    # these are docs that the doc name does not directly correspond to a component
    # may be a frontmatter flag could be set for these
    if folder == "binary_sensor":
        return file == "ttp229"
    elif folder == "climate":
        return file == "climate_ir"
    elif folder == "display":
        return file in [
            "lcd_display",
            "ssd1306",
            "ssd1322",
            "ssd1325",
            "ssd1327",
            "ssd1331",
            "ssd1351",
            "st7567",
        ]
    elif folder == "light":
        return file == "fastled"


def md_skip_imports(md_file, lines, index):
    # Skip import/export statements, committing position only after actual imports.
    # Blank lines between imports are consumed but don't commit the position.
    last_import_end = index
    i = index
    while i < len(lines):
        stripped = lines[i].strip()
        if stripped.startswith("import ") or stripped.startswith("export "):
            i += 1
            last_import_end = i  # commit: we've passed an import line
        elif not stripped:
            i += 1  # tentatively skip blank lines
        else:
            break  # non-import content reached

    return last_import_end


def oddities_titles(folder, file, title):
    # this replaces some titles which should be named otherwise
    if folder == "light":
        if file == "fastled":
            if title == "Clockless":
                return "fastled_clockless Component"
            elif title == "SPI":
                return "fastled_spi Component"
    elif folder == "components":
        if file == "dfrobot_sen0395":
            if title == "Hub Component":
                return "Component/Hub"
        elif file == "sn74hc595":
            if title == "Over SPI":
                # this is actually a typed schema, something to better figure documenting later
                return ""

    return title


def parse_file(md_full_path):
    lines = mrkdwn_lines(md_full_path)
    index = md_parse_frontmatter(md_full_path, lines)
    index = md_skip_imports(md_full_path, lines, index)
    see_also.reset_doc(md_full_path)
    file_name = md_full_path.stem
    file_folder = md_full_path.parent.name
    # doc_type captures what kind of schema we're documenting in the current context.
    # It is set from the file path, then updated as component/platform titles are processed.
    #   "component"          - root component  (components/api.mdx)
    #                          schema: api.json["api"]["schemas"]["CONFIG_SCHEMA"]
    #   "platform_index"     - platform base   (sensor/index.mdx)
    #                          schema: sensor.json["sensor"]["schemas"]["_SENSOR_SCHEMA"]
    #   "platform_component" - platform entry  (sensor/dallas_temp.mdx)
    #                          schema: dallas_temp.json["dallas_temp.sensor"]["schemas"]["CONFIG_SCHEMA"]
    doc_type = None
    doc_component = None  # component name — also the JSON file stem for schema lookup
    # component docs:
    # some components have .mdx files in folders, e.g. http_request
    # so for the root component (in core) we need to use the one in root, and ignore the one in subfolder,
    # that one will be used in e.g. sensors.json (platform)

    if file_name == "index" and file_folder == "components":
        return  # nothing here

    if file_name in core["components"]:
        # fill root component docs
        index, success = process_component(md_full_path, lines, index, file_name)
        if success:
            doc_type = "component"
            doc_component = file_name
    elif file_folder != "content" and file_folder in core["platforms"]:
        if file_name == "index":
            # fill core platform docs, from index files in platforms folders
            index, docs = md_get_paragraph(lines, index)
            core["platforms"][file_folder][JSON_DOCS] = convert_links(
                md_full_path, index, docs
            )
            stats.core_platform_docs += 1
            doc_type = "platform_index"
            doc_component = file_folder
        else:
            # this is a component inside a folder
            if not oddities_doc_not_specific_component(file_folder, file_name):
                index, success = process_platform_component(
                    md_full_path, lines, index, file_folder, file_name
                )
                if success:
                    doc_type = "platform_component"
                    doc_component = file_name
    elif file_folder == "automations":
        doc_component = "core"
    elif file_folder == "filter":
        parent_platform = md_full_path.parent.parent.name
        if parent_platform in core["platforms"]:
            doc_component = parent_platform

    doc_platform = file_folder if file_folder != "components" else None

    pending_schema = None

    while True:
        index, title = md_get_next_title(md_full_path, lines, index)
        if not title:
            break
        title_component = None

        title = oddities_titles(file_folder, file_name, title)
        if title == "Component/Hub":
            # Some files like pn523, rc522, as3935 are in a platform folder even
            # though they are full components and their platform components are
            # documented with the platform titles
            doc_platform = None

        elif title.endswith(" Component"):
            title_component = (
                title.replace(" Component", "")
                .replace("`", "")
                .replace(".", "")
                .lower()
            )
        elif title.endswith(DOC_OVER_SPI):
            title_component = f"{file_name}_spi"
        elif title.endswith(DOC_OVER_I2C):
            title_component = f"{file_name}_i2c"
        elif (
            # Handle Platform titles, e.g. Sensor, Switch titles
            file_name != "index"
            and get_platform_from_title(title, doc_component or file_name) is not None
        ):
            title_component = file_name
            doc_platform = get_platform_from_title(title, doc_component or file_name)

        if (
            title.endswith(" Action") or title.endswith(" Condition")
        ) and title.startswith("`"):
            config_type = title.split(" ")[-1].lower()  # action / condition
            parts = title.split(" ")[0].replace("`", "").split(".")
            if len(parts) == 1:
                # action; the component should be actual component
                if not doc_component:
                    print(f"{md_full_path}:{index} {title} with no config component.")
                    continue
                action_json = json_get(doc_component)
                if not action_json:
                    print(
                        f"{md_full_path}:{index} Found title {title} in {doc_component} cannot find config"
                    )
                else:
                    pending_schema = (
                        action_json.get(doc_component, {})
                        .get(config_type, {})
                        .get(parts[0])
                    )
            elif len(parts) == 2:
                # component.action
                pending_schema = (
                    (json_get(parts[0]) or {})
                    .get(parts[0], {})
                    .get(config_type, {})
                    .get(parts[1])
                )
            elif len(parts) == 3:
                # platform.component.action or # component.[action.name]
                if is_platform(parts[0]):
                    pending_schema = (
                        (json_get(parts[1]) or {})
                        .get(f"{parts[1]}.{parts[0]}", {})
                        .get(config_type, {})
                        .get(parts[2])
                    )
                else:
                    pending_schema = (
                        (json_get(parts[0]) or {})
                        .get(parts[0], {})
                        .get(config_type, {})
                        .get(".".join(parts[1:]))
                    )

            else:
                print(f"{md_full_path}:{index} Found title {title} too many parts")

            if pending_schema is not None:
                index, docs = md_get_paragraph(lines, index)
                pending_schema[JSON_DOCS] = convert_links(md_full_path, index, docs)
                if config_type == "action":
                    stats.action_docs += 1
                elif config_type == "condition":
                    stats.condition_docs += 1
            else:
                print(
                    f"{md_full_path}:{index} Found title {title} in {doc_component} config not found"
                )

        registry_key, registry_entry = find_registry_entry(title, doc_component)
        if registry_entry is None and doc_platform and doc_platform != doc_component:
            registry_key, registry_entry = find_registry_entry(title, doc_platform)
        if registry_entry is not None:
            index, docs = md_get_paragraph(lines, index)
            if docs:
                registry_entry[JSON_DOCS] = convert_links(md_full_path, index, docs)
            pending_schema = registry_entry
            stats.registry_docs += 1

        if title_component:
            if doc_platform in core["platforms"]:
                index, success = process_platform_component(
                    md_full_path, lines, index, doc_platform, title_component
                )
                if success:
                    doc_type = "platform_component"
                    doc_component = title_component
                elif title_component in core["components"]:
                    index, success = process_component(
                        md_full_path, lines, index, title_component
                    )
                    if success:
                        doc_type = "component"
                        doc_component = title_component
                    else:
                        print(
                            f"{md_full_path}:{index} {doc_platform}/{file_name} {title} not processed."
                        )
                else:
                    print(
                        f"{md_full_path}:{index} {doc_platform}/{file_name} {title} not processed."
                    )
            elif title_component in core["components"]:
                index, success = process_component(
                    md_full_path, lines, index, title_component
                )
                if success:
                    doc_type = "component"
                    doc_component = title_component
                else:
                    print(
                        f"{md_full_path}:{index} {doc_platform}/{file_name} {title} not processed."
                    )
            else:
                print(
                    f"{md_full_path}:{index} {doc_platform}/{file_name} {title} not processed."
                )

        if title == DOC_CONFIGURATION_VARIABLES:
            if not doc_component:
                print(
                    f"{md_full_path}:{index} TODO {doc_platform}/{file_name} {title} not processed."
                )
                continue

            if pending_schema:
                schema = pending_schema
            elif doc_type == "component":
                schema = (
                    (json_get(doc_component) or {})
                    .get(doc_component, {})
                    .get("schemas", {})
                    .get("CONFIG_SCHEMA")
                )
                if not schema:
                    print(
                        f"{md_full_path}:{index} {doc_component} CONFIG_SCHEMA not found"
                    )
            elif doc_type == "platform_index":
                all_schemas = (
                    (json_get(doc_component) or {})
                    .get(doc_component, {})
                    .get("schemas", {})
                )
                schema = all_schemas.get(
                    f"{doc_component.upper()}_SCHEMA"
                ) or all_schemas.get(f"_{doc_component.upper()}_SCHEMA")
            elif doc_type == "platform_component":
                schema = (
                    (json_get(doc_component) or {})
                    .get(f"{doc_component}.{doc_platform}", {})
                    .get("schemas", {})
                    .get("CONFIG_SCHEMA")
                )
                if not schema:
                    print(
                        f"{md_full_path}:{index} {doc_component}.{doc_platform} schema not found"
                    )
            else:
                schema = None
            if schema:
                try:
                    index = process_config(md_full_path, lines, index + 1, schema)
                except Exception as err:
                    print(f"{md_full_path}:{index} {title} failed {repr(err)}")
                    # if you put a breakpoint here get call-stack in the console by entering
                    # import traceback
                    # traceback.print_exc()
                    break
            pending_schema = None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Add docs to ESPHome json schema")
    parser.add_argument(
        "schema_dir", help="Directory to write filled JSON schema files"
    )
    parser.add_argument(
        "--read-schema-dir",
        help="Directory to read schema JSON files from (defaults to schema_dir)",
        default=None,
    )
    parser.add_argument("--single", help="Process a single json file", default=None)
    parser.add_argument(
        "--debug-level",
        help="Print parsing issues level, 0 prints nothing",
        default=0,
        type=int,
    )
    parser.add_argument(
        "--deploy-url",
        help="The base url for the deployment, e.g. https://esphome.io",
        default="https://esphome.io",
    )
    parser.add_argument(
        "--api-docs-url",
        help="The base url for api docs, e.g. https://api-docs.esphome.io",
        default="https://api-docs.esphome.io",
    )
    args = parser.parse_args()
    if args.read_schema_dir is None:
        args.read_schema_dir = args.schema_dir

    esphome_json = json_get("esphome")
    core = esphome_json["core"]

    # Pre-load all schema JSON files and build reverse extends map
    for json_file in Path(args.read_schema_dir).glob("*.json"):
        json_get(json_file.stem)
    fill_reverse_extends()

    md_full_paths = []
    for root, _, files in os.walk(DOCS_ROOT / "components"):
        for file in sorted(files):
            if file.endswith(".mdx"):
                fullpath = Path(root, file)
                md_full_paths.append(fullpath)
    md_full_paths.append(DOCS_ROOT / "automations" / "actions.mdx")

    if args.single:
        md_full_paths = [f for f in md_full_paths if args.single in repr(f)]

    # parse first index (platforms) so docs are filled in there first and not overriden later
    for md_full_path in md_full_paths:
        if md_full_path.stem == "index":
            parse_file(md_full_path)

    for md_full_path in md_full_paths:
        if md_full_path.stem != "index":
            parse_file(md_full_path)

    json_save()

    def attributes(obj):
        disallowed_names = {
            name
            for name, value in getmembers(type(obj))
            if isinstance(value, FunctionType)
        }
        return {
            name: getattr(obj, name)
            for name in dir(obj)
            if name[0] != "_" and name not in disallowed_names and hasattr(obj, name)
        }

    def print_attributes(obj):
        pprint(attributes(obj))

    print_attributes(stats)
