import jinja2
from jinja2.environment import Environment

from . import filters as custom_filters

__ENVIRONMENT__: Environment = None

CODEOWNERS = ["@corvis", "@esphome/core"]

FILTER_FUNCTION_PREFIX = "filter_"


class TemplateRenderingError(Exception):
    def __init__(self, msg: str, original_error: Exception) -> None:
        msg += ": " + str(original_error)
        super().__init__(msg)
        self.original_error = original_error


def _get_jinja2_env() -> Environment:
    global __ENVIRONMENT__
    if __ENVIRONMENT__ is None:
        __ENVIRONMENT__ = Environment()
        # Configuring filters
        for filter_fn_name in dir(custom_filters):
            if filter_fn_name.startswith(FILTER_FUNCTION_PREFIX):
                filter_name = filter_fn_name.replace(FILTER_FUNCTION_PREFIX, "", 1)
                filter_fn = getattr(custom_filters, filter_fn_name)
                if callable(filter_fn):
                    __ENVIRONMENT__.filters[filter_name] = getattr(
                        custom_filters, filter_fn_name
                    )

    return __ENVIRONMENT__


def contains_expression(value) -> bool:
    return "{{" in value or "{%" in value


def _create_template_for_str(str_val: str) -> jinja2.Template:
    # Configure template instance with required filters, extensions, blocks here
    return _get_jinja2_env().from_string(str_val, template_class=jinja2.Template)


def process_expression_value(value, context: dict):
    if not isinstance(value, str):
        return value  # Skip non-string values
    if not contains_expression(value):
        return value  # If there are no Jinja2 tags - just skip it too
    try:
        template = _create_template_for_str(value)
        return template.render(context)
    except jinja2.TemplateError as e:
        raise TemplateRenderingError(
            'Error in expression value "{}"'.format(value), e
        ) from e
