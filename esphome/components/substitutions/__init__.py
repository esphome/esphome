from collections import ChainMap
import logging
from re import Match
from typing import Any

from esphome import core
from esphome.config_helpers import Extend, Remove, merge_config, merge_dicts_ordered
import esphome.config_validation as cv
from esphome.const import CONF_SUBSTITUTIONS, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.util import OrderedDict
from esphome.yaml_util import (
    ConfigContext,
    ESPHomeDataBase,
    ESPLiteralValue,
    make_data_base,
)

from .jinja import Jinja, JinjaError, Missing, Resolver, UndefinedError, has_jinja

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)

ContextVars = ChainMap[str, Any]
SubstitutionPath = list[int | str]
ErrList = list[tuple[UndefinedError, SubstitutionPath, str]]
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
    path: SubstitutionPath,
    context_vars: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None,
) -> Any:
    if has_jinja(value):
        try:
            # Invoke the jinja engine to evaluate the expression.
            value = jinja.expand(value, context_vars)
        except UndefinedError as err:
            if strict_undefined:
                raise err
            if errors is not None:
                errors.append((err, path, value))
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
    value: str,
    path: SubstitutionPath,
    context_vars: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None,
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
                value, orig_value, path, context_vars, strict_undefined, errors
            )
            break

        i, j = m.span(0)
        name: str = m.group(1)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]
        sub: Any = context_vars.get(name, Missing)
        if sub is Missing:
            # Try see if there is a resolver
            resolver = context_vars.get(Resolver)
            if resolver:
                sub = resolver(name)

            if sub is Missing:
                err = UndefinedError(f"'{name}' is undefined")
                if strict_undefined:
                    raise err
                if errors is not None:
                    errors.append((err, path, value))
                i = j
                continue

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


def _push_context(
    local_vars: dict[str, Any],
    parent_context: ContextVars,
    errors: ErrList | None = None,
) -> tuple[ContextVars, dict[str, Any]]:
    """Returns a new context vars mapping with the given vars overriding those of the parent context, along
    with a version of `local_vars` resolved and sorted in dependency order.
    The below loops iterate exactly once if vars are already sorted in dependency order, i.e.,
    no var depends on another var defined later. Otherwise dependencies are resolved
    recursively.
    """
    unresolved_vars = local_vars.copy()
    resolved_vars = OrderedDict()
    context_vars = parent_context.new_child(resolved_vars)

    # Contains vars that could not be resolved due to missing or circular dependencies.
    unresolvables: dict[str, Any] = {}

    resolver_context = context_vars.new_child()

    def resolve(key: str) -> Any:
        """Resolves the given variable, recursively resolving dependencies as needed."""
        value = unresolved_vars.pop(key, Missing)
        if value is Missing:
            return Missing
        try:
            result = _substitute_item(value, [], resolver_context, True)
            if result is not None:
                value = result
        except UndefinedError:
            unresolvables[key] = value
            return Missing

        resolved_vars[key] = value
        return value

    # Set up the resolver for use during substitution
    resolver_context[Resolver] = resolve

    # Resolve all variables, recursively resolving dependencies as needed.
    # Each call to resolve() resolves that variable and any variables it depends on.
    while unresolved_vars:
        resolve(next(iter(unresolved_vars)))

    resolved_vars.update(unresolvables)

    if errors is not None:
        for name, value in unresolvables.items():
            err = UndefinedError(
                f"Could not resolve substitution variable '{name}' due to missing or circular dependencies.",
            )
            errors.append((err, [], value))

    return context_vars, resolved_vars


def push_context(
    config_node: Any,
    parent_context: ContextVars,
    errors: ErrList | None = None,
) -> ContextVars:
    """Returns the context vars this config node must be evaluated with."""
    if isinstance(config_node, ConfigContext):
        return _push_context(config_node.vars, parent_context, errors)[0]

    # This node does not define any vars itself, so just return parent context
    return parent_context


def _substitute_item(
    item: Any,
    path: SubstitutionPath,
    parent_context: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None = None,
) -> Any | None:
    if isinstance(item, ESPLiteralValue):
        return None  # do not substitute inside literal blocks

    # Push the current item's context onto the context stack
    context_vars = push_context(item, parent_context)

    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = _substitute_item(
                it, path + [i], context_vars, strict_undefined, errors
            )
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            if path or k != CONF_SUBSTITUTIONS:
                sub = _substitute_item(
                    k, path + [k], context_vars, strict_undefined, errors
                )
                if sub is not None:
                    replace_keys.append((k, sub))
            sub = _substitute_item(
                v, path + [k], context_vars, strict_undefined, errors
            )
            if sub is not None:
                item[k] = sub
        for old, new in replace_keys:
            if str(new) == str(old):
                item[new] = item[old]
            else:
                item[new] = merge_config(item.get(new), item.get(old))
                del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(item, path, context_vars, strict_undefined, errors)
        if not isinstance(sub, str) or sub != item:
            return sub
    elif isinstance(item, (core.Lambda, Extend, Remove)) and item.value:
        sub = _expand_substitutions(
            item.value, path, context_vars, strict_undefined, errors
        )
        if sub != item:
            item.value = sub
    return None


def _log_errors(errors: ErrList) -> None:
    for err, path, expression in errors:
        if "password" in path:
            continue
        location: str = "->".join(str(x) for x in path)
        if isinstance(expression, ESPHomeDataBase):
            location += f" in {str(expression.esp_range.start_mark)}"

        _LOGGER.warning(
            "The string '%s' looks like an expression,"
            " but could not resolve all the variables: %s (see %s)",
            expression,
            err.message,
            location,
        )


def do_substitution_pass(
    config: dict, command_line_substitutions: dict | None = None
) -> dict:
    # Extract substitutions from config, overriding with substitutions coming from command line:
    # Use merge_dicts_ordered to preserve OrderedDict type for move_to_end()
    substitutions = config.pop(CONF_SUBSTITUTIONS, {})
    with cv.prepend_path(CONF_SUBSTITUTIONS):
        if not isinstance(substitutions, dict):
            raise cv.Invalid(
                f"Substitutions must be a key to value mapping, got {type(substitutions)}"
            )
        substitutions = merge_dicts_ordered(
            substitutions, command_line_substitutions or {}
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

    errors: ErrList = []  # Collect undefined errors during substitution
    parent_context, substitutions = _push_context(substitutions, ContextVars(), errors)

    _substitute_item(config, [], parent_context, False, errors)

    if errors:
        _log_errors(errors)

    if (
        substitutions
    ):  # for readability, restore substitutions, if any, to front of dict
        config[CONF_SUBSTITUTIONS] = substitutions
        config.move_to_end(CONF_SUBSTITUTIONS, last=False)
    return config
