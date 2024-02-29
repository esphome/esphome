import logging

import esphome.config_validation as cv
from esphome import core
from esphome.const import CONF_SUBSTITUTIONS, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.yaml_util import ESPHomeDataBase, make_data_base
from esphome.config_helpers import merge_config

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)


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
                f"Substitution must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        validate_substitution_key: cv.Any(None, str, int, float, dict, list),
    }
)


async def to_code(config):
    pass


def _find_tokens(value):
    """
    Finds substitutable tokens in the form of:
    ```
    "a variable $a and a variable $abc"
    ```
    and turns them into pairs of:
    ```
    ('a', (11, 2)), ('abc', (16, 4))
    ```
    where the first number represents the characters
    skipped from the last token to the start of the new one
    and the last number the length of the token
    (including $ or ${})
    """
    if "$" not in value:
        return

    last_end = 0
    for match in cv.VARIABLE_PROG.finditer(value):
        name = match.group(1)
        start, end = match.span(0)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]

        yield match.group(1), (start - last_end, end - start)
        last_end = end


def _expand_substitutions(substitutions, value, path, ignore_missing):
    substituted = ""
    start_from = 0
    for name, (ignored_chars, length) in _find_tokens(value):
        if name not in substitutions:
            if not ignore_missing and "password" not in path:
                _LOGGER.warning(
                    "Found '%s' (see %s) which looks like a substitution, but '%s' was "
                    "not declared",
                    value,
                    "->".join(str(x) for x in path),
                    name,
                )
            substituted += value[start_from : ignored_chars + length]
            start_from = ignored_chars + length
            continue

        sub = substitutions[name]
        if isinstance(sub, str):
            substituted += value[start_from:ignored_chars] + sub
            start_from += ignored_chars + length
            continue

        if length != len(value):
            raise cv.Invalid(
                "String interpolation is only allowed for substitutions with "
                f"string types, however {name!r} (used in {'->'.join(str(x) for x in path)}) "
                f"is of type {type(sub)}"
            )

        return sub

    substituted += value[start_from:]

    # value can also already be a lambda with esp_range info, and only
    # a plain string is sent in value
    if isinstance(value, ESPHomeDataBase):
        # even though string can get larger or smaller, the range should point
        # to original document marks
        return make_data_base(substituted, value)

    return substituted


def _substitute_item(substitutions, item, path, ignore_missing):
    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = _substitute_item(substitutions, it, path + [i], ignore_missing)
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            # if we're not in the substitutions section, substitute keys
            if path or k != CONF_SUBSTITUTIONS:
                sub = _substitute_item(substitutions, k, path + [k], ignore_missing)
                if sub is not None:
                    replace_keys.append((k, sub))
            sub = _substitute_item(substitutions, v, path + [k], ignore_missing)
            if sub is not None:
                item[k] = sub
        for old, new in replace_keys:
            item[new] = merge_config(item.get(old), item.get(new))
            del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(substitutions, item, path, ignore_missing)
        if sub != item:
            return sub
    elif isinstance(item, core.Lambda):
        sub = _expand_substitutions(substitutions, item.value, path, ignore_missing)
        if sub != item:
            item.value = sub
    return None


def do_substitution_pass(config, command_line_substitutions, ignore_missing=False):
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
                f"Substitutions must be a key to value mapping, got {type(substitutions)}"
            )

        replace_keys = []
        for key, value in substitutions.items():
            with cv.prepend_path(key):
                sub = validate_substitution_key(key)
                if sub != key:
                    replace_keys.append((key, sub))
                substitutions[key] = value
        for old, new in replace_keys:
            substitutions[new] = substitutions[old]
            del substitutions[old]

    config[CONF_SUBSTITUTIONS] = substitutions
    # Move substitutions to the first place to replace substitutions in them correctly
    config.move_to_end(CONF_SUBSTITUTIONS, False)
    _substitute_item(substitutions, config, [], ignore_missing)
