from collections import ChainMap
import logging
from re import Match
from typing import Any

from esphome import core
from esphome.config_helpers import Extend, Remove, merge_config, merge_dicts_ordered
import esphome.config_validation as cv
from esphome.const import CONF_SUBSTITUTIONS, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.yaml_util import (
    ConfigContext,
    ESPHomeDataBase,
    ESPLiteralValue,
    add_context,
    make_data_base,
)

from .jinja import Jinja, JinjaError, UndefinedError, has_jinja

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)

ContextVars = ChainMap[str, Any]
jinja = Jinja()


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
        validate_substitution_key: object,
    }
)


async def to_code(config):
    pass


def _restore_data_base(value: Any, orig_value: ESPHomeDataBase) -> ESPHomeDataBase:
    """This function restores ESPHomeDataBase metadata held by the original string.
    This is needed because during jinja evaluation, strings can be replaced by other types,
    but we want to keep the original metadata for error reporting and source mapping.
    For example, if a substitution replaces a string with a dictionary, we want that items
    in the dictionary to still point to the original document location
    """
    if isinstance(value, ESPHomeDataBase):
        return value
    if isinstance(value, dict):
        return {
            _restore_data_base(k, orig_value): _restore_data_base(v, orig_value)
            for k, v in value.items()
        }
    if isinstance(value, list):
        return [_restore_data_base(v, orig_value) for v in value]
    if isinstance(value, str):
        return make_data_base(value, orig_value)
    return value


def _expand_jinja(
    value: str,
    orig_value: str,
    path,
    context_vars: ContextVars,
    strict_undefined: bool,
) -> Any:
    if has_jinja(value):
        try:
            # Invoke the jinja engine to evaluate the expression.
            value = jinja.expand(value, context_vars, strict_undefined)
        except UndefinedError as err:
            if strict_undefined:
                raise err
            if "password" not in path:
                _LOGGER.warning(
                    "Found '%s' (see %s) which looks like an expression,"
                    " but could not resolve all the variables: %s",
                    value,
                    "->".join(str(x) for x in path),
                    err.message,
                )
            return value
        except JinjaError as err:
            raise cv.Invalid(
                f"{err.error_name()} Error evaluating jinja expression '{value}': {str(err.parent())}."
                f"\nEvaluation stack: (most recent evaluation last)\n{err.stack_trace_str()}"
                f"\nRelevant context:\n{err.context_trace_str()}"
                f"\nSee {'->'.join(str(x) for x in path)}",
                path,
            )
        # If the original, unexpanded string, contained document metadata (ESPHomeDatabase),
        # assign this same document metadata to the resulting value.
        if isinstance(orig_value, ESPHomeDataBase):
            value = _restore_data_base(value, orig_value)

    return value


def _expand_substitutions(
    value: str, path, context_vars: ContextVars, strict_undefined: bool
) -> Any:
    if "$" not in value:
        return value

    orig_value = value

    i = 0
    while True:
        m: Match[str] = cv.VARIABLE_PROG.search(value, i)
        if not m:
            # No more variable substitutions found. See if the remainder looks like a jinja template
            value = _expand_jinja(
                value, orig_value, path, context_vars, strict_undefined
            )
            break

        i, j = m.span(0)
        name: str = m.group(1)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]
        if name not in context_vars:
            if strict_undefined:
                raise UndefinedError(f"'{name}' is undefined")
            if "password" not in path:
                _LOGGER.warning(
                    "Found '%s' (see %s) which looks like a substitution, but '%s' was "
                    "not declared",
                    orig_value,
                    "->".join(str(x) for x in path),
                    name,
                )
            i = j
            continue

        sub: Any = context_vars[name]

        if i == 0 and j == len(value):
            # The variable spans the whole expression, e.g., "${varName}". Return its resolved value directly
            # to conserve its type.
            value = sub
            break

        tail = value[j:]
        value = value[:i] + str(sub)
        i = len(value)
        value += tail

    # orig_value can also already be a lambda with esp_range info, and only
    # a plain string is sent in orig_value
    if isinstance(orig_value, ESPHomeDataBase):
        # even though string can get larger or smaller, the range should point
        # to original document marks
        value = make_data_base(value, orig_value)

    return value


def push_context(config_node: Any, parent_context: ContextVars) -> ContextVars:
    """Returns the context_vars this config node must be evaluated with.
    If any var definition contains substitutions itself, those are resolved first using
    the values from parent_context.
    """
    if not isinstance(config_node, ConfigContext):
        # This node does not define any vars itself, so just return parent context
        return parent_context
    vars = config_node.vars
    unresolved_vars = vars.copy()
    vars.clear()
    context_vars = parent_context.new_child(vars)
    # below loops iterate exactly once if vars are already sorted in dependency order, i.e.,
    # no var depends on another var defined later. If there are dependency cycles or
    # unresolved vars, multiple passes are done until no progress is made.
    while unresolved_vars:
        new_unresolved_vars = {}
        for k, value in unresolved_vars.items():
            try:
                result = substitute(value, [], context_vars, True)
            except UndefinedError:  # try to resolve in next pass
                new_unresolved_vars[k] = value
                continue
            vars[k] = value if result is None else result
        if len(unresolved_vars) == len(new_unresolved_vars):
            vars.update(unresolved_vars)
            break
        unresolved_vars = new_unresolved_vars
    return context_vars


def substitute(
    item: Any,
    path: list[int | str],
    parent_context: ContextVars,
    strict_undefined: bool,
) -> Any | None:
    """Applies substitutions in-place to the given item.
    If a substitution was made to `item` itself, the new value is returned.
    Returns None if no substitution was made to `item` itself."""
    if isinstance(item, ESPLiteralValue):
        return None  # do not substitute inside literal blocks
    context_vars = push_context(item, parent_context)
    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = substitute(it, path + [i], context_vars, strict_undefined)
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            if path or k != CONF_SUBSTITUTIONS:
                sub = substitute(k, path + [k], context_vars, strict_undefined)
                if sub is not None:
                    replace_keys.append((k, sub))
                sub = substitute(v, path + [k], context_vars, strict_undefined)
                if sub is not None:
                    item[k] = sub
        for old, new in replace_keys:
            if str(new) == str(old):
                item[new] = item[old]
            else:
                item[new] = merge_config(item.get(old), item.get(new))
                del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(item, path, context_vars, strict_undefined)
        if sub != item:
            return sub
    elif isinstance(item, (core.Lambda, Extend, Remove)) and item.value:
        sub = _expand_substitutions(item.value, path, context_vars, strict_undefined)
        if sub != item:
            item.value = sub

    return None


def do_substitution_pass(
    config: dict, command_line_substitutions: dict | None = None
) -> None:
    # Merge substitutions in config, overriding with substitutions coming from command line:
    # Use merge_dicts_ordered to preserve OrderedDict type for move_to_end()
    substitutions = merge_dicts_ordered(
        config.get(CONF_SUBSTITUTIONS, {}), command_line_substitutions or {}
    )
    with cv.prepend_path(CONF_SUBSTITUTIONS):
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

    if CONF_SUBSTITUTIONS in config:
        config[CONF_SUBSTITUTIONS] = substitutions
        config.move_to_end(CONF_SUBSTITUTIONS, last=False)
    config = add_context(config, substitutions)
    substitute(config, [], ContextVars(), False)
