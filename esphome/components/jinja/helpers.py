import re

from esphome.yaml_util import ESPHomeDataBase, make_data_base

DETECT_JINJA = r"(\$\{)"
detect_jinja_re = re.compile(
    r"<%.+?%>"  # Block form expression: <% ... %>
    r"|\$\{[^}]+\}",  # Braced form expression: ${ ... }
    flags=re.MULTILINE,
)


def has_jinja(st):
    return (
        isinstance(st, JinjaStr)
        or isinstance(st, str)
        and detect_jinja_re.search(st) is not None
    )


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

    @staticmethod
    def merge(old, new):
        st = JinjaStr(
            r"${merge(eval(__old), eval(__new))}", {"__old": old, "__new": new}
        )
        if isinstance(new, ESPHomeDataBase):
            return make_data_base(st, new)
        if isinstance(old, ESPHomeDataBase):
            return make_data_base(st, old)
        return st
