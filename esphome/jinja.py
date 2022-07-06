import re
from jinja2.nativetypes import NativeEnvironment
import esphome.config_validation as cv

VALID_SUBSTITUTIONS_CHARACTERS = (
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
)
# pylint: disable=consider-using-f-string
VARIABLE_PROG = re.compile(f"\\$([{VALID_SUBSTITUTIONS_CHARACTERS}]+)")


DETECT_JINJA = r"(\$\{)|(\{%)|(^\s*%%)"
jinja_detect_re = re.compile(DETECT_JINJA, flags=re.MULTILINE)


def has_jinja(st):
    return jinja_detect_re.search(st) is not None


jinja = NativeEnvironment(
    trim_blocks=True,
    lstrip_blocks=True,
    line_statement_prefix="%%",
    line_comment_prefix="##",
    variable_start_string="${",
    variable_end_string="}",
)

jinja.add_extension("jinja2.ext.do")


def _validate_var_name(key):
    with cv.prepend_path(key):
        key = cv.string(key)
        if not key:
            raise cv.Invalid("Variable name must not be empty")
        if key[0] == "$":
            key = key[1:]
        if key[0].isdigit():
            raise cv.Invalid("First character in substitutions cannot be a digit.")
        for char in key:
            if char not in VALID_SUBSTITUTIONS_CHARACTERS:
                raise cv.Invalid(
                    f"Variable name must only consist of upper/lowercase characters, the underscore and numbers. The character '{char}' cannot be used"
                )
        return key


def validate_vars(vars):
    return {_validate_var_name(key): value for key, value in vars.items()}


def expand_str(st, vars):
    def replace_vars(m):
        name = m.group(1)
        return f"${{{name}}}" if name in vars else f"${name}"

    # replace $var with ${var} so jinja catches it below:
    result = VARIABLE_PROG.sub(replace_vars, st)
    if has_jinja(result):
        template = jinja.from_string(result)
        result = template.render(vars)

    return result
