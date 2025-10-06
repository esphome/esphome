from ast import literal_eval
from collections import ChainMap
from collections.abc import Iterator
from itertools import chain, islice
import logging
import math
from types import GeneratorType
from typing import Any

import jinja2 as jinja
from jinja2.nativetypes import NativeCodeGenerator, NativeTemplate
import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import CONF_JINJA, VALID_SUBSTITUTIONS_CHARACTERS
from esphome.yaml_util import ESPHomeDataBase, ESPLiteralValue, make_data_base

from .helpers import JinjaStr, has_jinja

TemplateError = jinja.TemplateError
TemplateSyntaxError = jinja.TemplateSyntaxError
TemplateRuntimeError = jinja.TemplateRuntimeError
UndefinedError = jinja.UndefinedError
Undefined = jinja.Undefined

CODEOWNERS = ["@jpeletier"]
_LOGGER = logging.getLogger(__name__)

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


def is_jinja_enabled(config):
    return CONF_JINJA in config


def validate_identifier(value):
    value = cv.string(value)
    if not value:
        raise cv.Invalid("Identifier name must not be empty")
    if value[0].isdigit():
        raise cv.Invalid("First character in an identifier cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise cv.Invalid(
                f"Jinja identifier names must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
            )
    return value


def _merge_return_into_body(obj):
    """
    Combines the value of "return" into the macro body
    """
    params = obj.get("parameters", {})
    vars = obj.get("vars", {})
    body = obj.get("body", "")
    ret = obj.get("return")

    if ret is not None:
        # wrap the return value
        ret_stmt = f"${{{ret}}}"
        body = f"{body}\n{ret_stmt}" if body else ret_stmt

    return {"parameters": params, "body": body, "vars": vars}


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("macros"): cv.Schema(
            {
                validate_identifier: cv.All(
                    {
                        cv.Optional("parameters"): cv.ensure_schema(
                            cv.Schema({validate_identifier: object})
                        ),
                        cv.Optional("vars"): dict,
                        cv.Optional("body"): cv.string,
                        cv.Optional("return"): cv.string,
                    },
                    _merge_return_into_body,
                )
            },
            extra=vol.PREVENT_EXTRA,
        ),
        cv.Optional("vars"): cv.ensure_schema({validate_identifier: object}),
    }
)


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

    def __init__(self, config: dict, context_vars: dict):
        from esphome.config_helpers import merge_config

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

        def jinja_eval(expr, ctx=None):
            if isinstance(expr, dict):
                expr = dict(expr)
                for k, v in expr.items():
                    new_k = jinja_eval(k, ctx)
                    v = jinja_eval(v, ctx)
                    if new_k != k:
                        expr.pop(k)
                        k = new_k
                    expr[k] = v
                return expr
            if isinstance(expr, list):
                return [jinja_eval(v, ctx) for v in expr]
            if not isinstance(expr, str):
                return expr
            result, _ = self.expand(JinjaStr(expr, ctx))
            if isinstance(expr, ESPHomeDataBase):
                result = make_data_base(result, expr)
            return result

        self.globals = {
            **self.globals,
            **self.context_vars,
            "eval": jinja_eval,
            "merge": merge_config,
            **SAFE_GLOBALS,
        }
        if CONF_JINJA in config:
            with cv.prepend_path(CONF_JINJA):
                config[CONF_JINJA] = CONFIG_SCHEMA(config[CONF_JINJA])

            jinja_config = config[CONF_JINJA]
            if "vars" in jinja_config:
                self.load_vars(jinja_config["vars"])
            if "macros" in jinja_config:
                self.load_macros(jinja_config["macros"])

        self.globals = {**self.globals, **self.context_vars}

    def parse_template(self, content, upvalues, imports=None):
        local_env = self
        if len(upvalues) > 0:
            local_env = self.overlay()
            local_env.globals = ChainMap(upvalues, self.globals)
        template = local_env.from_string(content)
        if imports is None:
            # import all symbols
            for symbol_name in dir(template.module):
                if symbol_name.startswith("_"):
                    continue
                self.globals[symbol_name] = getattr(template.module, symbol_name)
        else:
            for symbol_name in imports:
                symbol = getattr(template.module, symbol_name)
                if symbol is not None:
                    self.globals[symbol_name] = symbol

    def load_vars(self, vars):
        """
        Adds variables only visible to Jinja. Note that substitutions
        have precedence and will override these
        """
        for var_name, value in vars.items():
            if var_name not in self.context_vars:
                self.context_vars[var_name] = value

    def load_macros(self, macros):
        """
        Creates Jinja macros out of a simplified yaml syntax
        """
        for name, macro in macros.items():
            # parameters contains a dict of parameter names to default values
            parameters = macro["parameters"] or {}
            macro["vars"] = macro_vars = {
                **self.context_vars,
                **macro.get("vars", {}),
            }
            body = macro["body"]
            local_env = self
            if len(macro_vars) > 0:
                local_env = self.overlay()
                local_env.globals = ChainMap(macro_vars, self.globals)
            template = local_env.from_string(body)

            def make_macro_func(template=template, parameters=parameters, name=name):
                keys = tuple(parameters.keys())

                def macro_func(*args, **kwargs):
                    call_params = {**parameters}  # copy defaults
                    for i, arg in enumerate(args):
                        if i < len(keys):
                            call_params[keys[i]] = arg
                    for k, v in kwargs.items():
                        if k in call_params:
                            call_params[k] = v
                    return template.render(call_params)

                return macro_func

            self.globals[name] = make_macro_func()

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
