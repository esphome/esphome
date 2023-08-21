# Copyright (c) Kuba Szczodrzyński 2023-06-01.

# pylint: skip-file
# flake8: noqa

import json
import re
from pathlib import Path

from black import FileMode, format_str
from ltchiptool import Board, Family
from ltchiptool.util.lvm import LVM

BASE_CODE_INIT = """
# This file was auto-generated by libretiny/generate_components.py
# Do not modify its contents.
# For custom pin validators, put validate_pin() or validate_usage()
# in gpio.py file in this directory.
# For changing schema/pin schema, put COMPONENT_SCHEMA or COMPONENT_PIN_SCHEMA
# in schema.py file in this directory.

from esphome import pins
from esphome.components import libretiny
from esphome.components.libretiny.const import (
    COMPONENT_{COMPONENT},
    CONF_LIBRETINY,
    KEY_COMPONENT_DATA,
    KEY_LIBRETINY,
    LibreTinyComponent,
)
from esphome.core import CORE

{IMPORTS}

CODEOWNERS = ["@kuba2k2"]
AUTO_LOAD = ["libretiny"]

COMPONENT_DATA = LibreTinyComponent(
    name=COMPONENT_{COMPONENT},
    boards={COMPONENT}_BOARDS,
    board_pins={COMPONENT}_BOARD_PINS,
    pin_validation={PIN_VALIDATION},
    usage_validation={USAGE_VALIDATION},
)


def _set_core_data(config):
    CORE.data[KEY_LIBRETINY] = {}
    CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA] = COMPONENT_DATA
    return config


CONFIG_SCHEMA = {SCHEMA}

PIN_SCHEMA = {PIN_SCHEMA}

CONFIG_SCHEMA.prepend_extra(_set_core_data)


async def to_code(config):
    return await libretiny.component_to_code(config)


@pins.PIN_SCHEMA_REGISTRY.register("{COMPONENT_LOWER}", PIN_SCHEMA)
async def pin_to_code(config):
    return await libretiny.gpio.component_pin_to_code(config)
"""

BASE_CODE_BOARDS = """
# This file was auto-generated by libretiny/generate_components.py
# Do not modify its contents.

from esphome.components.libretiny.const import {FAMILIES}

{COMPONENT}_BOARDS = {BOARDS_JSON}

{COMPONENT}_BOARD_PINS = {PINS_JSON}

BOARDS = {COMPONENT}_BOARDS
"""

# variable names in component extension code
VAR_SCHEMA = "COMPONENT_SCHEMA"
VAR_PIN_SCHEMA = "COMPONENT_PIN_SCHEMA"
VAR_GPIO_PIN = "validate_pin"
VAR_GPIO_USAGE = "validate_usage"

# lines for code snippets
SCHEMA_BASE = "libretiny.BASE_SCHEMA"
SCHEMA_EXTRA = f"libretiny.BASE_SCHEMA.extend({VAR_SCHEMA})"
PIN_SCHEMA_BASE = "libretiny.gpio.BASE_PIN_SCHEMA"
PIN_SCHEMA_EXTRA = f"libretiny.BASE_PIN_SCHEMA.extend({VAR_PIN_SCHEMA})"

# supported root components
COMPONENT_MAP = {
    "rtl87xx": "realtek-amb",
    "bk72xx": "beken-72xx",
}


def subst(code: str, key: str, value: str) -> str:
    return code.replace(f"{{{key}}}", value)


def subst_all(code: str, value: str) -> str:
    return re.sub(r"{.+?}", value, code)


def subst_many(code: str, *templates: tuple[str, str]) -> str:
    while True:
        prev_code = code
        for key, value in templates:
            code = subst(code, key, value)
        if code == prev_code:
            break
    return code


def check_base_code(code: str) -> None:
    code = subst_all(code, "DUMMY")
    formatted = format_str(code, mode=FileMode())
    if code.strip() != formatted.strip():
        print(formatted)
        raise RuntimeError("Base code is not formatted properly")


def write_component_code(
    component_dir: Path,
    component: str,
) -> None:
    code = BASE_CODE_INIT
    gpio_py = component_dir.joinpath("gpio.py")
    schema_py = component_dir.joinpath("schema.py")
    init_py = component_dir.joinpath("__init__.py")

    # gather all imports
    imports = {
        "gpio": set(),
        "schema": set(),
        "boards": {"{COMPONENT}_BOARDS", "{COMPONENT}_BOARD_PINS"},
    }
    # substitution values
    values = dict(
        COMPONENT=component.upper(),
        COMPONENT_LOWER=component.lower(),
        SCHEMA=SCHEMA_BASE,
        PIN_SCHEMA=PIN_SCHEMA_BASE,
        PIN_VALIDATION="None",
        USAGE_VALIDATION="None",
    )

    # parse gpio.py file to find custom validators
    if gpio_py.is_file():
        gpio_code = gpio_py.read_text()
        if VAR_GPIO_PIN in gpio_code:
            values["PIN_VALIDATION"] = VAR_GPIO_PIN
            imports["gpio"].add(VAR_GPIO_PIN)

    # parse schema.py file to find schema extension
    if schema_py.is_file():
        schema_code = schema_py.read_text()
        if VAR_SCHEMA in schema_code:
            values["SCHEMA"] = SCHEMA_EXTRA
            imports["schema"].add(VAR_SCHEMA)
        if VAR_PIN_SCHEMA in schema_code:
            values["PIN_SCHEMA"] = PIN_SCHEMA_EXTRA
            imports["schema"].add(VAR_PIN_SCHEMA)

    # add import lines if needed
    import_lines = "\n".join(
        f"from .{m} import {', '.join(sorted(v))}" for m, v in imports.items() if v
    )
    code = subst_many(
        code,
        ("IMPORTS", import_lines),
        *values.items(),
    )
    # format with black
    code = format_str(code, mode=FileMode())
    # write back to file
    init_py.write_text(code)


def write_component_boards(
    component_dir: Path,
    component: str,
    boards: list[Board],
) -> list[Family]:
    code = BASE_CODE_BOARDS
    variants_dir = Path(LVM.path(), "boards", "variants")
    boards_py = component_dir.joinpath("boards.py")
    pin_regex = r"#define PIN_(\w+)\s+(\d+)"
    pin_number_regex = r"0*(\d+)$"

    # families to import
    families = set()
    # found root families
    root_families = []
    # substitution values
    values = dict(
        COMPONENT=component.upper(),
    )
    # resulting JSON objects
    boards_json = {}
    pins_json = {}

    # go through all boards found for this root family
    for board in boards:
        family = "FAMILY_" + board.family.short_name
        boards_json[board.name] = {
            "name": board.title,
            "family": family,
        }
        families.add(family)
        if board.family not in root_families:
            root_families.append(board.family)

        board_h = variants_dir.joinpath(f"{board.name}.h")
        board_code = board_h.read_text()
        board_pins = {}
        for match in re.finditer(pin_regex, board_code):
            pin_name = match[1]
            pin_value = match[2]
            board_pins[pin_name] = int(pin_value)
            # trim leading zeroes in GPIO numbers
            pin_name = re.sub(pin_number_regex, r"\1", pin_name)
            board_pins[pin_name] = int(pin_value)
        pins_json[board.name] = board_pins

    # make the JSONs format as non-inline
    boards_json = json.dumps(boards_json).replace("}", ",}")
    pins_json = json.dumps(pins_json).replace("}", ",}")
    # remove quotes from family constants
    for family in families:
        boards_json = boards_json.replace(f'"{family}"', family)
    code = subst_many(
        code,
        ("FAMILIES", ", ".join(sorted(families))),
        ("BOARDS_JSON", boards_json),
        ("PINS_JSON", pins_json),
        *values.items(),
    )
    # format with black
    code = format_str(code, mode=FileMode())
    # write back to file
    boards_py.write_text(code)
    return root_families


def write_const(
    components_dir: Path,
    components: set[str],
    families: dict[str, str],
) -> None:
    const_py = components_dir.joinpath("libretiny").joinpath("const.py")
    if not const_py.is_file():
        raise FileNotFoundError(const_py)
    code = const_py.read_text()
    components = sorted(components)
    v2f = families
    families = sorted(families)

    # regex for finding the component list block
    comp_regex = r"(# COMPONENTS.+?\n)(.*?)(\n# COMPONENTS)"
    # build component constants
    comp_str = "\n".join(f'COMPONENT_{f} = "{f.lower()}"' for f in components)
    # replace the 2nd regex group only
    repl = lambda m: m.group(1) + comp_str + m.group(3)
    code = re.sub(comp_regex, repl, code, flags=re.DOTALL | re.MULTILINE)

    # regex for finding the family list block
    fam_regex = r"(# FAMILIES.+?\n)(.*?)(\n# FAMILIES)"
    # build family constants
    fam_defs = "\n".join(f'FAMILY_{v} = "{v}"' for v in families)
    fam_list = ", ".join(f"FAMILY_{v}" for v in families)
    fam_friendly = ", ".join(f'FAMILY_{v}: "{v}"' for v in families)
    fam_component = ", ".join(f"FAMILY_{v}: COMPONENT_{v2f[v]}" for v in families)
    fam_lines = [
        fam_defs,
        "FAMILIES = [",
        fam_list,
        ",]",
        "FAMILY_FRIENDLY = {",
        fam_friendly,
        ",}",
        "FAMILY_COMPONENT = {",
        fam_component,
        ",}",
    ]
    var_str = "\n".join(fam_lines)
    # replace the 2nd regex group only
    repl = lambda m: m.group(1) + var_str + m.group(3)
    code = re.sub(fam_regex, repl, code, flags=re.DOTALL | re.MULTILINE)

    # format with black
    code = format_str(code, mode=FileMode())
    # write back to file
    const_py.write_text(code)


if __name__ == "__main__":
    # safety check if code is properly formatted
    check_base_code(BASE_CODE_INIT)
    # list all boards from ltchiptool
    components_dir = Path(__file__).parent.parent
    boards = [Board(b) for b in Board.get_list()]
    # keep track of all supported root- and chip-families
    components = set()
    families = {}
    # loop through supported components
    for component, family_name in COMPONENT_MAP.items():
        family = Family.get(name=family_name)
        # make family component directory
        component_dir = components_dir.joinpath(component)
        component_dir.mkdir(exist_ok=True)
        # filter boards list
        family_boards = [b for b in boards if family in b.family.inheritance]
        # write __init__.py
        write_component_code(component_dir, component)
        # write boards.py
        component_families = write_component_boards(
            component_dir, component, family_boards
        )
        # store current root component name
        components.add(component.upper())
        # add all chip families
        for family in component_families:
            families[family.short_name] = component.upper()
    # update libretiny/const.py
    write_const(components_dir, components, families)
