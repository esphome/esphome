from ast import literal_eval
from collections.abc import Iterator
from itertools import chain, islice
import logging
import math
import re
from types import GeneratorType
from typing import Any

import jinja2 as jinja
from jinja2.nativetypes import NativeCodeGenerator, NativeTemplate

from esphome.yaml_util import ESPLiteralValue

TemplateError = jinja.TemplateError
TemplateSyntaxError = jinja.TemplateSyntaxError
TemplateRuntimeError = jinja.TemplateRuntimeError
UndefinedError = jinja.UndefinedError
Undefined = jinja.Undefined

_LOGGER = logging.getLogger(__name__)

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


class JinjaStr(str):
    """
    Wraps a string containing an unresolved Jinja expression,
    storing the variables visible to it when it failed to resolve.
    For example, an expression inside a package, `${ A * B }` may fail
    to resolve at package parsing time if `A` is a local package var
    but `B` is a substitution defined in the root yaml.
    Therefore, we store the value of `A` as an upvalue bound
    to the original string so we may be able to resolve `${ A * B }`
    later in the main substitutions pass.
    """

    Undefined = object()

    def __new__(cls, value: str, upvalues=None):
        if isinstance(value, JinjaStr):
            base = str(value)
            merged = {**value.upvalues, **(upvalues or {})}
        else:
            base = value
            merged = dict(upvalues or {})
        obj = super().__new__(cls, base)
        obj.upvalues = merged
        obj.result = JinjaStr.Undefined
        return obj


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
        if isinstance(val, JinjaStr):
            self.environment.context_trace[key] = val
            val, _ = self.environment.expand(val)
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
    """
    Wraps a Jinja environment
    """

    # jinja environment customization overrides
    code_generator_class = NativeCodeGenerator
    concat = staticmethod(_concat_nodes_override)

    def __init__(self, context_vars: dict):
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
        self.context_vars = {**context_vars}
        for k, v in self.context_vars.items():
            if isinstance(v, ESPLiteralValue):
                continue
            if isinstance(v, str) and not isinstance(v, JinjaStr) and has_jinja(v):
                self.context_vars[k] = JinjaStr(v, self.context_vars)

        self.globals = {
            **self.globals,
            **self.context_vars,
            **SAFE_GLOBALS,
        }

    def expand(self, content_str: str | JinjaStr) -> Any:
        """
        Renders a string that may contain Jinja expressions or statements
        Returns the resulting value if all variables and expressions could be resolved.
        Otherwise, it returns a tagged (JinjaStr) string that captures variables
        in scope (upvalues), like a closure for later evaluation.
        """
        result = None
        override_vars = {}
        if isinstance(content_str, JinjaStr):
            if content_str.result is not JinjaStr.Undefined:
                return content_str.result, None
            # If `value` is already a JinjaStr, it means we are trying to evaluate it again
            # in a parent pass.
            # Hopefully, all required variables are visible now.
            override_vars = content_str.upvalues

        old_trace = self.context_trace
        self.context_trace = {}
        try:
            template = self.from_string(content_str)
            result = template.render(override_vars)
            if isinstance(result, Undefined):
                print("" + result)  # force a UndefinedError exception
        except (TemplateSyntaxError, UndefinedError) as err:
            # `content_str` contains a Jinja expression that refers to a variable that is undefined
            # in this scope. Perhaps it refers to a root substitution that is not visible yet.
            # Therefore, return `content_str` as a JinjaStr, which contains the variables
            # that are actually visible to it at this point to postpone evaluation.
            return JinjaStr(content_str, {**self.context_vars, **override_vars}), err
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

        if isinstance(content_str, JinjaStr):
            content_str.result = result

        return result, None


class JinjaTemplate(NativeTemplate):
    environment_class = Jinja


Jinja.template_class = JinjaTemplate
