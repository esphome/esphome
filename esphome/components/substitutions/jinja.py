import logging
import math
import re
import jinja2 as jinja
from jinja2.nativetypes import NativeEnvironment

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


def has_jinja(st):
    return detect_jinja_re.search(st) is not None


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

    def __new__(cls, value: str, upvalues=None):
        obj = super().__new__(cls, value)
        obj.upvalues = upvalues or {}
        return obj

    def __init__(self, value: str, upvalues=None):
        self.upvalues = upvalues or {}


class Jinja:
    """
    Wraps a Jinja environment
    """

    def __init__(self, context_vars):
        self.env = NativeEnvironment(
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
        self.env.add_extension("jinja2.ext.do")
        self.env.globals["math"] = math  # Inject entire math module
        self.context_vars = {**context_vars}
        self.env.globals = {**self.env.globals, **self.context_vars}

    def expand(self, content_str):
        """
        Renders a string that may contain Jinja expressions or statements
        Returns the resulting processed string if all values could be resolved.
        Otherwise, it returns a tagged (JinjaStr) string that captures variables
        in scope (upvalues), like a closure for later evaluation.
        """
        result = None
        override_vars = {}
        if isinstance(content_str, JinjaStr):
            # If `value` is already a JinjaStr, it means we are trying to evaluate it again
            # in a parent pass.
            # Hopefully, all required variables are visible now.
            override_vars = content_str.upvalues
        try:
            template = self.env.from_string(content_str)
            result = template.render(override_vars)
            if isinstance(result, Undefined):
                # This happens when the expression is simply an undefined variable. Jinja does not
                # raise an exception, instead we get "Undefined".
                # Trigger an UndefinedError exception so we skip to below.
                print("" + result)
        except (TemplateSyntaxError, UndefinedError) as err:
            # `content_str` contains a Jinja expression that refers to a variable that is undefined
            # in this scope. Perhaps it refers to a root substitution that is not visible yet.
            # Therefore, return the original `content_str` as a JinjaStr, which contains the variables
            # that are actually visible to it at this point to postpone evaluation.
            return JinjaStr(content_str, {**self.context_vars, **override_vars}), err

        return result, None
