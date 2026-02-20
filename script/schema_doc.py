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
    enum_docs = 0
    action_docs = 0
    condition_docs = 0
    missing_anchors: list = None

    def __init__(self):
        self.missing_anchors = []


stats = Stats()

anchors = {}
md_docs = {}
json_docs = {}


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


REGEX_INCLUDE = r"^{{<\sinclude\s\"([^\"]*)\"\s>}}"


def mrkdwn_lines_includes(lines, md_file):
    ret_lines = []
    for index in range(0, len(lines)):
        line = lines[index]
        search = re.search(REGEX_INCLUDE, line, re.IGNORECASE)
        if search:
            include_path = md_file.parent / search.group(1)
            if not include_path.exists():
                print(f"{md_file}:{index + 1} cannot include {include_path}")
                continue

            include_lines = open_file_lines(include_path)
            include_index = md_parse_frontmatter(None, include_lines)
            ret_lines.extend(include_lines[include_index:])
        else:
            ret_lines.append(line)
    return ret_lines


def mrkdwn_lines(md_file):
    lines = md_docs.get(md_file, {}).get("lines")
    if lines:
        return lines

    if (lines := open_file_lines(md_file)) is not None:
        lines = mrkdwn_lines_includes(lines, md_file)
        # cache into md_docs dict
        md_docs[md_file] = {"lines": lines}
        return lines


def fill_anchors(md_files):
    REGEX_SPAN_ANCHOR = r'<span\s+id="([^"]*)"'
    REGEX_HEADING = r"^#{1,6}\s+(.*)"
    for md_file in md_files:
        lines = mrkdwn_lines(md_file)
        for line in lines:
            search = re.search(REGEX_SPAN_ANCHOR, line)
            if search:
                anchors[search.group(1)] = md_file
            search = re.search(REGEX_HEADING, line)
            if search:
                slug = slugify(search.group(1))
                if slug not in anchors:
                    anchors[slug] = md_file


def get_doc_title(md_file):
    title = md_docs.get(md_file, {}).get("title")
    if title:
        return title

    lines = mrkdwn_lines(md_file)
    md_parse_frontmatter(md_file, lines)

    return md_docs.get(md_file, {}).get("title")


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
            elif args.debug_level > 3:
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


def json_exists(name):
    json_file_name = os.path.join(args.schema_dir, name + ".json")
    if os.path.exists(json_file_name):
        return True
    return False


def json_get(name):
    if name == "core":
        name = "esphome"

    json_doc = json_docs.get(name)
    if json_doc:
        return json_doc

    json_file_name = os.path.join(args.schema_dir, name + ".json")
    if os.path.exists(json_file_name):
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
    core["components"][name][JSON_DOCS] = make_doc_with_see_also(md_file, index, docs)
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


def get_platform_from_title(title, config_component=None):
    esphome_json = json_get("esphome")
    title = title.lower().replace("`", "")
    if config_component and title.startswith(config_component.lower()):
        title = title[len(config_component) + 1 :]
    name = title.replace(" ", "_")
    if name in esphome_json["core"]["platforms"]:
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


def convert_links(md_file, index, docs):
    if docs is None:
        return None

    # Matches [name-group-1](#local-link-group-2)
    REGEX_LOCAL_LINK = r"\[([^\]]*)\]\(#([^\)]*)\)"

    def replacer_local(match):
        title = match.group(1)
        anchor = match.group(2)
        if anchor not in anchors:
            if anchor not in stats.missing_anchors:
                stats.missing_anchors.append(anchor)
            url = anchor
        else:
            anchor_file = anchors[anchor]
            relative = anchor_file.relative_to(DOCS_ROOT)
            url_path = "/" + "/".join(relative.parts[:-1])
            if anchor_file.stem != "index":
                url_path += f"/{anchor_file.stem}"
            url = f"{args.deploy_url}{url_path}#{anchor}"

        return f"[{title}]({url})"

    return re.sub(REGEX_LOCAL_LINK, replacer_local, docs)


def set_schema_doc(md_file, index, schema, prop_name, prop_types, doc):
    TYPE_TEMPLATABLE = "[templatable](#config-templatable)"

    matched_config = find_schema_prop(schema, prop_name)
    if matched_config:
        converted_doc = make_doc_with_see_also(md_file, index, doc)

        if prop_types:
            type_parts = [part.strip() for part in prop_types.split(",")]
            optionality = type_parts[0].replace("*", "").lower()
            config_optionality = matched_config.get(JSON_KEY, "")
            if (
                prop_name != "id"
                and optionality != config_optionality.lower()
                and args.debug_level > 3
            ):
                print(
                    f"{md_file}:{index} {prop_name} Key {config_optionality} in ESPHome does not match {optionality} in docs"
                )

            templatable = TYPE_TEMPLATABLE in type_parts[1:]
            config_templatable = matched_config.get(JSON_TEMPLATABLE, False)
            if templatable != config_templatable and args.debug_level > 3:
                print(
                    f"{md_file}:{index} {prop_name} Templatable {config_templatable} in ESPHome does not match {templatable} in docs"
                )

            # Document with type information, unless the type just says templatable
            if len(type_parts) > 1 and type_parts[1] != TYPE_TEMPLATABLE:
                prop_type = convert_links(md_file, index, type_parts[1])
                matched_config[JSON_DOCS] = f"**{prop_type}**: {converted_doc}"
                stats.props += 1
                return matched_config

        matched_config[JSON_DOCS] = converted_doc

        stats.props += 1
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
                if args.debug_level > 2:
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Add docs to ESPHome json schema")
    parser.add_argument("schema_dir", help="Directory containing JSON files")
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

    esphome_json = json_get("esphome")
    core = esphome_json["core"]

    md_files = []
    for root, _, files in os.walk(DOCS_ROOT / "components"):
        for file in files:
            if file.endswith(".mdx"):
                fullpath = Path(root, file)
                md_files.append(fullpath)
    md_files.append(DOCS_ROOT / "automations" / "actions.mdx")

    fill_anchors(
        md_files
        + [
            # config-lambda, config-templatable
            DOCS_ROOT / "automations" / "templates.mdx",
            # config-id, config-pin_schema
            DOCS_ROOT / "guides" / "configuration-types.mdx",
            # api-rest
            DOCS_ROOT / "web-api" / "index.mdx",
        ]
    )

    if args.single:
        md_files = [f for f in md_files if args.single in repr(f)]

    for md_file in md_files:
        lines = mrkdwn_lines(md_file)
        index = md_parse_frontmatter(md_file, lines)
        see_also.reset_doc(md_file)
        file_name = md_file.stem
        content_folder = md_file.parent.name
        is_platform = False
        is_component = False
        config_component = None
        json_config = None
        # component docs:
        # some components have .mdx files in folders, e.g. http_request
        # so for the root component (in core) we need to use the one in root, and ignore the one in subfolder,
        # that one will be used in e.g. sensors.json (platform)

        if file_name == "index" and content_folder == "components":
            continue  # nothing here

        if file_name in core["components"]:
            # fill root component docs
            index, is_component = process_component(md_file, lines, index, file_name)
            if is_component:
                config_component = file_name
        elif content_folder != "content" and content_folder in core["platforms"]:
            if file_name == "index":
                # fill core platform docs, from index files in platforms folders
                index, docs = md_get_paragraph(lines, index)
                core["platforms"][content_folder][JSON_DOCS] = convert_links(
                    md_file, index, docs
                )
                stats.core_platform_docs += 1
                is_platform = True
                config_component = content_folder
            else:
                # this is a component inside a folder
                if not oddities_doc_not_specific_component(content_folder, file_name):
                    index, is_platform = process_platform_component(
                        md_file, lines, index, content_folder, file_name
                    )
                    if is_platform:
                        config_component = file_name
        elif content_folder == "automations":
            config_component = "core"

        platform_name = content_folder if content_folder != "components" else None
        title_config_vars = None

        while True:
            index, title = md_get_next_title(md_file, lines, index)
            if not title:
                break
            component_name = None

            title = oddities_titles(content_folder, file_name, title)
            if title == "Component/Hub":
                # Some files like pn523, rc522, as3935 are in a platform folder even
                # though they are full components and their platform components are
                # documented with the platform titles
                platform_name = None

            elif title.endswith(" Component"):
                component_name = (
                    title.replace(" Component", "")
                    .replace("`", "")
                    .replace(".", "")
                    .lower()
                )
            elif title.endswith(DOC_OVER_SPI):
                component_name = f"{file_name}_spi"
            elif title.endswith(DOC_OVER_I2C):
                component_name = f"{file_name}_i2c"
            elif (
                # Handle Platform titles, e.g. Sensor, Switch titles
                file_name != "index"
                and get_platform_from_title(title, config_component or file_name)
                is not None
            ):
                component_name = file_name
                platform_name = get_platform_from_title(
                    title, config_component or file_name
                )

            if (
                title.endswith(" Action") or title.endswith(" Condition")
            ) and title.startswith("`"):
                config_type = title.split(" ")[-1].lower()  # action / condition
                parts = title.split(" ")[0].replace("`", "").split(".")
                if len(parts) == 1:
                    # action; the component should be actual component
                    if not config_component:
                        print(f"{md_file}:{index} {title} with no config component.")
                        continue
                    if json_config != json_get(config_component):
                        print(f"{md_file}:{index} {title} set needed for this.")
                    json_config = json_get(config_component)
                    if not json_config:
                        print(
                            f"{md_file}:{index} Found title {title} in {config_component} cannot find config"
                        )
                    else:
                        title_config_vars = (
                            json_config.get(config_component, {})
                            .get(config_type, {})
                            .get(parts[0])
                        )
                elif len(parts) == 2:
                    # component.action
                    title_config_vars = (
                        (json_get(parts[0]) or {})
                        .get(parts[0], {})
                        .get(config_type, {})
                        .get(parts[1])
                    )
                elif len(parts) == 3:
                    # platform.component.action
                    if parts[1] not in core["components"]:
                        print(
                            f"{md_file}:{index} Found {config_type} {title} with invalid name format"
                        )
                    title_config_vars = (
                        (json_get(parts[1]) or {})
                        .get(f"{parts[1]}.{parts[0]}", {})
                        .get(config_type, {})
                        .get(parts[2])
                    )

                else:
                    print(f"{md_file}:{index} Found title {title} too many parts")

                if title_config_vars is not None:
                    index, docs = md_get_paragraph(lines, index)
                    title_config_vars[JSON_DOCS] = convert_links(md_file, index, docs)
                    if config_type == "action":
                        stats.action_docs += 1
                    elif config_type == "condition":
                        stats.condition_docs += 1
                else:
                    print(
                        f"{md_file}:{index} Found title {title} in {config_component} config not found"
                    )

            if component_name:
                is_platform = platform_name in core["platforms"]
                is_component = False
                if is_platform:
                    index, is_platform = process_platform_component(
                        md_file, lines, index, platform_name, component_name
                    )

                if not is_platform and component_name in core["components"]:
                    index, is_component = process_component(
                        md_file, lines, index, component_name
                    )

                if not is_platform and not is_component:
                    print(
                        f"{md_file}:{index} {platform_name}/{file_name} {title} not processed."
                    )
                else:
                    config_component = component_name

            if title == DOC_CONFIGURATION_VARIABLES:
                if not config_component:
                    print(
                        f"{md_file}:{index} TODO {platform_name}/{file_name} {title} not processed."
                    )
                    continue

                if title_config_vars:
                    schema = title_config_vars
                else:
                    json_config = json_get(config_component)
                    if not json_config:
                        print(f"{md_file}:{index} {config_component} no json_config")
                        schema = None
                    elif is_component:
                        schema = json_config[config_component]["schemas"][
                            "CONFIG_SCHEMA"
                        ]
                    elif is_platform and config_component:
                        if config_component == platform_name:
                            schema = json_config[config_component]["schemas"].get(
                                f"{platform_name.upper()}_SCHEMA"
                            )
                        elif platform_name:
                            schema = json_config[f"{config_component}.{platform_name}"][
                                "schemas"
                            ].get("CONFIG_SCHEMA")
                        else:
                            print(
                                f"{md_file}:{index} {config_component} unknown component type"
                            )
                    else:
                        schema = None
                if schema:
                    try:
                        index = process_config(md_file, lines, index + 1, schema)
                    except Exception as err:
                        print(f"{md_file}:{index} {title} failed {repr(err)}")
                        # if you put a breakpoint here get call-stack in the console by entering
                        # import traceback
                        # traceback.print_exc()
                        break
                title_config_vars = None

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
