import logging
import re

import esphome.config_validation as cv
from esphome import core
from esphome.const import CONF_SUBSTITUTIONS
from esphome.yaml_util import ESPHomeDataBase, make_data_base

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)

VALID_SUBSTITUTIONS_CHARACTERS = (
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
)


def validate_substitution_key(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Substitution key must not be empty")
    if value[0] == "$":
        value = value[1:]
    if value[0].isdigit():
        raise cv.Invalid("First character in substitutions cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                "Substitution must only consist of upper/lowercase characters, the underscore "
                "and numbers. The character '{}' cannot be used".format(char)
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        validate_substitution_key: cv.string_strict,
    }
)


async def to_code(config):
    pass


VARIABLE_PROG = re.compile(
    "\\$([{0}]+|\\{{[{0}]*\\}})".format(VALID_SUBSTITUTIONS_CHARACTERS)
)


def _expand_substitutions(substitutions, value, path):
    if "$" not in value:
        return value

    orig_value = value

    i = 0
    while True:
        m = VARIABLE_PROG.search(value, i)
        if not m:
            # Nothing more to match. Done
            break

        i, j = m.span(0)
        name = m.group(1)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]
        if name not in substitutions:
            _LOGGER.warning(
                "Found '%s' (see %s) which looks like a substitution, but '%s' was "
                "not declared",
                orig_value,
                "->".join(str(x) for x in path),
                name,
            )
            i = j
            continue

        sub = substitutions[name]
        tail = value[j:]
        value = value[:i] + sub
        i = len(value)
        value += tail

    # orig_value can also already be a lambda with esp_range info, and only
    # a plain string is sent in orig_value
    if isinstance(orig_value, ESPHomeDataBase):
        # even though string can get larger or smaller, the range should point
        # to original document marks
        return make_data_base(value, orig_value)

    return value


def _substitute_item(substitutions, item, path):
    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = _substitute_item(substitutions, it, path + [i])
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            if path or k != CONF_SUBSTITUTIONS:
                sub = _substitute_item(substitutions, k, path + [k])
                if sub is not None:
                    replace_keys.append((k, sub))
            sub = _substitute_item(substitutions, v, path + [k])
            if sub is not None:
                item[k] = sub
        for old, new in replace_keys:
            item[new] = item[old]
            del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(substitutions, item, path)
        if sub != item:
            return sub
    elif isinstance(item, core.Lambda):
        sub = _expand_substitutions(substitutions, item.value, path)
        if sub != item:
            item.value = sub
    return None


def do_substitution_pass(config, command_line_substitutions):
    if CONF_SUBSTITUTIONS not in config and not command_line_substitutions:
        return

    substitutions = config[CONF_SUBSTITUTIONS]
    if substitutions is None:
        substitutions = command_line_substitutions
    elif command_line_substitutions:
        substitutions = {**substitutions, **command_line_substitutions}
    with cv.prepend_path("substitutions"):
        if not isinstance(substitutions, dict):
            raise cv.Invalid(
                "Substitutions must be a key to value mapping, got {}"
                "".format(type(substitutions))
            )

        replace_keys = []
        for key, value in substitutions.items():
            with cv.prepend_path(key):
                sub = validate_substitution_key(key)
                if sub != key:
                    replace_keys.append((key, sub))
                substitutions[key] = cv.string_strict(value)
        for old, new in replace_keys:
            substitutions[new] = substitutions[old]
            del substitutions[old]

    config[CONF_SUBSTITUTIONS] = substitutions
    # Move substitutions to the first place to replace substitutions in them correctly
    config.move_to_end(CONF_SUBSTITUTIONS, False)
    _substitute_item(substitutions, config, [])
