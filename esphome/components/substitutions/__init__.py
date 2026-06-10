from collections import ChainMap
import logging
from typing import Any

import esphome
from esphome import core
from esphome.config_helpers import Extend, Remove, merge_config, merge_dicts_ordered
import esphome.config_validation as cv
from esphome.const import CONF_SUBSTITUTIONS, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.types import ConfigType
from esphome.util import OrderedDict
from esphome.yaml_util import (
    ConfigContext,
    DocumentPath,
    ESPHomeDataBase,
    ESPLiteralValue,
    IncludeFile,
    format_path,
    make_data_base,
)

from .jinja import Jinja, JinjaError, Missing, Resolver, UndefinedError, has_jinja

CODEOWNERS = ["@esphome/core"]
_LOGGER = logging.getLogger(__name__)

ContextVars = ChainMap[str, Any]
ErrList = list[tuple[UndefinedError, DocumentPath, Any]]

# Module-level instance is safe: context_vars is passed per-call, and context_trace
# is stack-saved/restored within expand(). Not thread-safe — only use from one thread.
jinja = Jinja()


def raise_first_undefined(
    errors: ErrList,
    context_label: str,
) -> None:
    """If *errors* is non-empty, raise ``cv.Invalid`` for the first undefined variable.

    The raised error names the missing variable and its location in the include
    stack. Only the first error is surfaced; the user will re-run after fixing it
    and any remaining undefined variables will be reported then.

    ``context_label`` is the noun describing where the undefined variable
    appeared (e.g. ``"package definition"``).
    """
    if not errors:
        return
    err, err_path, err_value = errors[0]
    if len(errors) > 1:
        # Log any further undefined variables so debug-level output covers
        # the full set, even though only the first is surfaced to the user.
        extras = ", ".join(
            f"{e.message} at '{'->'.join(str(p) for p in p_path)}'"
            for e, p_path, _ in errors[1:]
        )
        _LOGGER.debug("Additional undefined variables in %s: %s", context_label, extras)
    raise cv.Invalid(
        f"Undefined variable in {context_label}: {err.message}\n{format_path(err_path, err_value)}"
    )


def validate_substitution_key(value: Any) -> str:
    """Validate and normalize a substitution key, stripping a leading ``$`` if present."""
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Substitution key must not be empty")
    if value[0] == "$":
        value = value[1:]
    if not value:
        raise cv.Invalid("Substitution key must not be empty")
    if value[0].isdigit():
        raise cv.Invalid("First character in substitutions cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                f"Substitution must only consist of upper/lowercase characters,"
                f" the underscore and numbers."
                f" The character '{char}' cannot be used"
            )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        validate_substitution_key: object,
    }
)


async def to_code(config: ConfigType) -> None:
    """No runtime code generation needed — substitutions are resolved at config time."""


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


def _resolve_var(name: str, context_vars: ContextVars) -> Any:
    """Look up a substitution variable, falling back to the resolver callback."""
    sub = context_vars.get(name, Missing)
    if sub is Missing:
        resolver = context_vars.get(Resolver)
        if resolver:
            sub = resolver(name)
    return sub


def _handle_undefined(
    err: UndefinedError,
    path: DocumentPath,
    value: Any,
    strict_undefined: bool,
    errors: ErrList | None,
) -> None:
    """Handle an undefined variable.

    In strict mode, raises immediately. Otherwise, appends to the errors
    list for deferred warning at the end of the substitution pass.
    """
    if strict_undefined:
        raise err
    if errors is not None:
        errors.append((err, path, value))


def _expand_substitutions(
    value: str,
    path: DocumentPath,
    context_vars: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None,
) -> Any:
    """Expand ``$var``, ``${var}``, and Jinja expressions in a string.

    Works in two phases:

    1. **Simple substitution** — scan for ``$name`` / ``${name}`` tokens
       and replace them with the value from *context_vars*.  If the token
       spans the entire string, return the raw value (preserving type).
    2. **Jinja evaluation** — if the result still contains Jinja syntax
       (e.g. ``${a * b}``), render it through the Jinja engine with the
       full *context_vars* as template variables.

    Returns the expanded value (may be a non-string type) or the
    original *value* unchanged if there is nothing to substitute.
    """
    if "$" not in value:
        return value

    orig_value = value

    # Phase 1: Replace $var and ${var} references
    search_pos = 0
    while (m := cv.VARIABLE_PROG.search(value, search_pos)) is not None:
        match_start, match_end = m.span(0)
        name: str = m.group(1)
        if name.startswith("{") and name.endswith("}"):
            name = name[1:-1]
        sub = _resolve_var(name, context_vars)
        if sub is Missing:
            _handle_undefined(
                err=UndefinedError(f"'{name}' is undefined"),
                path=path,
                value=value,
                strict_undefined=strict_undefined,
                errors=errors,
            )
            search_pos = match_end
            continue

        if match_start == 0 and match_end == len(value):
            # The variable spans the whole expression, e.g., "${varName}".
            # Return its resolved value directly to conserve its type.
            value = sub
            break

        tail = value[match_end:]
        value = value[:match_start] + str(sub)
        search_pos = len(value)
        value += tail

    # Phase 2: Evaluate any remaining jinja expressions (e.g., "${a * b}")
    if isinstance(value, str) and has_jinja(value):
        try:
            value = jinja.expand(value, context_vars)
        except UndefinedError as err:
            _handle_undefined(
                err=err,
                path=path,
                value=value,
                strict_undefined=strict_undefined,
                errors=errors,
            )
        except JinjaError as err:
            raise cv.Invalid(
                f"{err.error_name()} Error evaluating jinja expression"
                f" '{value}': {str(err.parent())}."
                f"\nEvaluation stack: (most recent evaluation last)"
                f"\n{err.stack_trace_str()}"
                f"\nRelevant context:\n{err.context_trace_str()}"
                f"\n{format_path(path, orig_value)}",
                path,
            ) from err
        else:
            if isinstance(orig_value, ESPHomeDataBase):
                value = _restore_data_base(value, orig_value)

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
    """Resolve local_vars and layer them on top of parent_context.

    Returns ``(child_context, resolved_vars)`` where *child_context* is a
    new :class:`ChainMap` whose front map is *resolved_vars* (an
    :class:`OrderedDict` of successfully-resolved variables).

    Variables may reference each other (e.g. ``b: ${a + 1}``).
    Dependencies are resolved recursively via a *resolver* callback
    that Jinja invokes on cache-miss.  If vars are already in
    dependency order, the loop iterates exactly once per variable.

    The ChainMap stack used during resolution is::

        resolver_context  →  resolved_vars  →  parent maps …
              ↑                    ↑
        holds Resolver       filled as vars
        callback               are resolved
    """
    # Vars still waiting to be resolved — popped one-by-one by resolve().
    unresolved_vars = local_vars.copy()
    # Accumulates resolved values in dependency order; becomes the front
    # map of the returned child context so later lookups find them first.
    resolved_vars = OrderedDict()
    # The context callees will search: resolved_vars (initially empty)
    # shadowing whatever the parent already provides.
    context_vars = parent_context.new_child(resolved_vars)

    # Vars that failed resolution (missing or circular references).
    # Maps name → (original_value, cause_error) for deferred warnings.
    unresolvables: dict[str, tuple[Any, UndefinedError]] = {}

    # One extra child layer so the Resolver callback lives in its own
    # map and doesn't pollute resolved_vars.
    resolver_context = context_vars.new_child()

    def resolve(key: str) -> Any:
        """Resolve a variable, recursively resolving any dependencies it references."""
        value = unresolved_vars.pop(key, Missing)
        if value is Missing:
            # Either already resolved (in resolved_vars) or currently being
            # resolved (self-reference from inside a dict-valued substitution).
            # Returning what we have lets sibling references inside a dict
            # value, e.g. ``${device.manufacturer}`` inside ``device.name``,
            # see literal sibling values during their own resolution.
            return resolved_vars.get(key, Missing)
        if isinstance(value, dict):
            # Dict-valued substitutions form a namespace; eagerly publish the
            # original mapping so its members can reference each other while
            # the dict's own substitution pass is still running. The entry is
            # replaced with the fully-substituted dict once recursion returns.
            resolved_vars[key] = value
        try:
            value = substitute(value, [], resolver_context, True)
        except UndefinedError as err:
            unresolvables[key] = (value, err)
            return Missing
        resolved_vars[key] = value
        return value

    # Set up the resolver for use during substitution
    resolver_context[Resolver] = resolve

    # Resolve all variables, recursively resolving dependencies as needed.
    # Each call to resolve() resolves that variable and any variables it depends on.
    while unresolved_vars:
        resolve(next(iter(unresolved_vars)))

    for name, (value, cause) in unresolvables.items():
        resolved_vars[name] = value
        if errors is not None:
            _handle_undefined(
                err=UndefinedError(
                    f"Could not resolve substitution variable '{name}': {cause}"
                ),
                path=["substitutions", name],
                value=value,
                strict_undefined=False,
                errors=errors,
            )

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


def resolve_include(
    include: IncludeFile,
    path: DocumentPath,
    context_vars: ContextVars,
    strict_undefined: bool = True,
    errors: ErrList | None = None,
) -> Any:
    """Resolve an include, substituting the filename if needed.

    Note: no path-traversal validation is performed on the resolved filename.
    A substitution that resolves to an absolute path will bypass the parent
    directory (Path.__truediv__ ignores the left operand for absolute paths).
    ESPHome's trust model assumes the config author controls all substitution
    values (including command-line substitutions), so path restrictions are
    an explicit non-goal here.
    """
    original = include.file
    original_str = str(original)
    filename = str(
        _expand_substitutions(
            original_str, path + ["file"], context_vars, strict_undefined, errors
        )
    )
    substituted = filename != original_str
    if substituted:
        include = IncludeFile(
            include.parent_file, filename, include.vars, include.yaml_loader
        )
    try:
        return include.load()
    except esphome.core.EsphomeError as err:
        resolved = f" (expanded from '{original}')" if substituted else ""
        raise cv.Invalid(
            f"Error including file '{filename}'{resolved}: {err}"
            f"\n{format_path(path, original)}",
            path + [f"<{filename}>"],
        ) from err


def _substitute_include(
    include: IncludeFile,
    path: DocumentPath,
    context_vars: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None,
) -> Any:
    """Resolve an include and substitute its content."""
    content = resolve_include(include, path, context_vars, strict_undefined, errors)
    return substitute(content, path, context_vars, strict_undefined, errors)


def substitute(
    item: Any,
    path: DocumentPath,
    parent_context: ContextVars,
    strict_undefined: bool,
    errors: ErrList | None = None,
) -> Any:
    """Returns a recursively substituted version of `item`."""

    if isinstance(item, ESPLiteralValue):
        return item  # do not substitute inside literal blocks

    # Push the current item's context onto the context stack
    context_vars = push_context(item, parent_context, errors)

    result = item

    if isinstance(item, list):
        result = [
            substitute(it, path + [i], context_vars, strict_undefined, errors)
            for i, it in enumerate(item)
        ]

    elif isinstance(item, dict):
        result = OrderedDict()
        for k, v in item.items():
            v = substitute(v, path + [k], context_vars, strict_undefined, errors)
            k = substitute(k, path + [k], context_vars, strict_undefined, errors)
            result[k] = merge_config(result.get(k), v)

    elif isinstance(item, str):
        result = _expand_substitutions(
            item, path, context_vars, strict_undefined, errors
        )

    elif isinstance(item, (core.Lambda, Extend, Remove)) and item.value:
        value = _expand_substitutions(
            item.value, path, context_vars, strict_undefined, errors
        )
        if item.value != value:
            result = type(item)(value)

    elif isinstance(item, IncludeFile):
        result = _substitute_include(item, path, context_vars, strict_undefined, errors)

    if isinstance(item, ESPHomeDataBase):
        result = make_data_base(result, item)
    return result


def _warn_unresolved_variables(errors: ErrList) -> None:
    """Log warnings for unresolved substitution variables, skipping password fields."""
    for err, path, expression in errors:
        if "password" in path:
            continue
        _LOGGER.warning(
            "The string '%s' looks like an expression,"
            " but could not resolve all the variables: %s\n%s",
            expression,
            err.message,
            format_path(path, expression),
        )


def resolve_substitutions_block(
    substitutions: Any,
    command_line_substitutions: dict[str, Any] | None,
) -> dict[str, Any]:
    """Resolve a deferred ``substitutions: !include file.yaml`` and validate the shape.

    The caller is responsible for wrapping the call in
    ``cv.prepend_path(CONF_SUBSTITUTIONS)`` for error reporting.
    ``command_line_substitutions`` seeds the filename context so
    ``substitutions: !include ${var}.yaml`` can reference CLI-provided vars.
    """
    if isinstance(substitutions, IncludeFile):
        # Single-shot resolution — matches ``_walk_packages`` for the
        # ``packages: !include`` entry point.  Chained includes (an include that
        # itself loads another ``!include`` at the top level) are not supported.
        substitutions = resolve_include(
            substitutions,
            [],
            ContextVars(command_line_substitutions or {}),
            strict_undefined=False,
        )
    if not isinstance(substitutions, dict):
        raise cv.Invalid(
            f"Substitutions must be a key to value mapping, got {type(substitutions)}"
        )
    return substitutions


def do_substitution_pass(
    config: OrderedDict, command_line_substitutions: dict[str, Any] | None = None
) -> OrderedDict:
    """Run the substitution pass over the entire config.

    Extracts the ``substitutions:`` block, merges in any command-line
    overrides, resolves inter-variable dependencies, then walks the
    config tree replacing all ``$var`` / ``${expr}`` references.
    Returns a new config dict with resolved substitutions
    restored at the front.
    """
    # Extract substitutions from config, overriding with substitutions coming from command line:
    # Use merge_dicts_ordered to preserve OrderedDict type for move_to_end()
    substitutions = config.pop(CONF_SUBSTITUTIONS, {})
    with cv.prepend_path(CONF_SUBSTITUTIONS):
        substitutions = resolve_substitutions_block(
            substitutions, command_line_substitutions
        )
        substitutions = merge_dicts_ordered(
            substitutions, command_line_substitutions or {}
        )

        replace_keys: list[tuple[str, str]] = []
        for key in substitutions:
            with cv.prepend_path(key):
                sub = validate_substitution_key(key)
                if sub != key:
                    replace_keys.append((key, sub))
        for old, new in replace_keys:
            substitutions[new] = substitutions[old]
            del substitutions[old]

    errors: ErrList = []  # Collect undefined errors during substitution
    parent_context, substitutions = _push_context(substitutions, ContextVars(), errors)

    config = substitute(config, [], parent_context, False, errors)

    if errors:
        _warn_unresolved_variables(errors)

    # Restore substitutions to front of dict for readability
    if substitutions:
        config[CONF_SUBSTITUTIONS] = substitutions
        config.move_to_end(CONF_SUBSTITUTIONS, last=False)
    return config
