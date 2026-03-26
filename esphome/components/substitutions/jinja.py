from ast import literal_eval
from collections.abc import Iterator, Mapping
from itertools import chain, islice
import math
import re
from types import GeneratorType
from typing import Any

import jinja2 as jinja
from jinja2.nativetypes import NativeCodeGenerator, NativeTemplate
from jinja2.runtime import missing as Missing

TemplateError = jinja.TemplateError
TemplateSyntaxError = jinja.TemplateSyntaxError
TemplateRuntimeError = jinja.TemplateRuntimeError
UndefinedError = jinja.UndefinedError
Undefined = jinja.Undefined
# Sentinel key for resolver callback in ContextVars.
# Dots are invalid in substitution names so this can never collide with user keys.
Resolver = ".resolver"


DETECT_JINJA = r"(\$\{)"
detect_jinja_re = re.compile(
    r"<%.+?%>"  # Block form expression: <% ... %>
    r"|\$\{[^}]+\}",  # Braced form expression: ${ ... }
    flags=re.MULTILINE,
)


def has_jinja(st: str) -> bool:
    return detect_jinja_re.search(st) is not None


# SAFE_GLOBALS defines a allowlist of built-in functions or modules that are considered safe to expose
# in Jinja templates or other sandboxed evaluation contexts. Only functions that do not allow
# arbitrary code execution, file access, or other security risks are included.
#
# The following functions are considered safe:
#   - math: The entire math module is injected, allowing access to mathematical functions like sin, cos, sqrt, etc.
#   - ord: Converts a character to its Unicode code point integer.
#   - chr: Converts an integer to its corresponding Unicode character.
#   - len: Returns the length of a sequence or collection.
#
# These functions were chosen because they are pure, have no side effects, and do not provide access
# to the file system, environment, or other potentially sensitive resources.
SAFE_GLOBALS = {
    "math": math,  # Inject entire math module
    "ord": ord,
    "chr": chr,
    "len": len,
}


class JinjaError(Exception):
    def __init__(self, context_trace: dict, expr: str):
        self.context_trace = context_trace
        self.eval_stack = [expr]

    def parent(self):
        return self.__context__

    def error_name(self):
        return type(self.parent()).__name__

    def context_trace_str(self):
        return "\n".join(
            f"  {k} = {repr(v)} ({type(v).__name__})"
            for k, v in self.context_trace.items()
        )

    def stack_trace_str(self):
        return "\n".join(
            f" {len(self.eval_stack) - i}: {expr}{i == 0 and ' <-- ' + self.error_name() or ''}"
            for i, expr in enumerate(self.eval_stack)
        )


class TrackerContext(jinja.runtime.Context):
    def resolve_or_missing(self, key):
        val = super().resolve_or_missing(key)
        if val is Missing:
            # Variable not in the template context — check if a resolver callback
            # was registered (by _push_context) to lazily resolve dependencies
            # between substitution variables in the same block.
            resolver = super().resolve_or_missing(Resolver)
            if resolver is not Missing:
                val = resolver(key)
        self.environment.context_trace[key] = val
        return val


def _concat_nodes_override(values: Iterator[Any]) -> Any:
    """
    This function customizes how Jinja preserves native types when concatenating
    multiple result nodes together. If the result is a single node, its value
    is returned. Otherwise, the nodes are concatenated as strings. If
    the result can be parsed with `ast.literal_eval`, the parsed
    value is returned. Otherwise, the string is returned.
    This helps preserve metadata such as ESPHomeDataBase from original values
    and mimicks how HomeAssistant deals with template evaluation and preserving
    the original datatype.
    """
    head: list[Any] = list(islice(values, 2))

    if not head:
        return None

    if len(head) == 1:
        raw = head[0]
        if not isinstance(raw, str):
            return raw
    else:
        if isinstance(values, GeneratorType):
            values = chain(head, values)
        raw = "".join([str(v) for v in values])

    result = None
    try:
        # Attempt to parse the concatenated string into a Python literal.
        # This allows expressions like "1 + 2" to be evaluated to the integer 3.
        # If the result is also a string or there is a parsing error,
        # fall back to returning the raw string. This is consistent with
        #  Home Assistant's behavior when evaluating templates
        result = literal_eval(raw)
    except (ValueError, SyntaxError, MemoryError, TypeError):
        pass
    else:
        if isinstance(result, set):
            # Sets are not supported, return raw string
            return raw

        if not isinstance(result, str):
            return result

    return raw


class Jinja(jinja.Environment):
    """Jinja environment configured for ESPHome substitution expressions."""

    # jinja environment customization overrides
    code_generator_class = NativeCodeGenerator
    concat = staticmethod(_concat_nodes_override)

    def __init__(self) -> None:
        super().__init__(
            trim_blocks=True,
            lstrip_blocks=True,
            block_start_string="<%",
            block_end_string="%>",
            line_statement_prefix="#",
            line_comment_prefix="##",
            variable_start_string="${",
            variable_end_string="}",
            undefined=jinja.StrictUndefined,
        )
        self.context_class = TrackerContext
        self.add_extension("jinja2.ext.do")
        self.context_trace = {}

        self.globals = {**self.globals, **SAFE_GLOBALS}

    def expand(self, content_str: str, context_vars: Mapping[str, Any]) -> Any:
        """
        Renders a string that may contain Jinja expressions or statements
        Returns the resulting value if all variables and expressions could be resolved.
        """
        result = None

        old_trace = self.context_trace
        self.context_trace = {}
        try:
            template = self.from_string(content_str)
            result = template.render(context_vars)
            if isinstance(result, Undefined):
                str(result)  # force a UndefinedError exception
        except UndefinedError as err:
            raise err
        except JinjaError as err:
            err.context_trace = {**self.context_trace, **err.context_trace}
            err.eval_stack.append(content_str)
            raise err
        except (
            TemplateError,
            TemplateRuntimeError,
            RuntimeError,
            ArithmeticError,
            AttributeError,
            TypeError,
        ) as err:
            raise JinjaError(self.context_trace, content_str) from err
        finally:
            self.context_trace = old_trace

        return result


class JinjaTemplate(NativeTemplate):
    environment_class = Jinja


Jinja.template_class = JinjaTemplate
