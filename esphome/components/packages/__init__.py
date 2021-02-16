import copy
from typing import Union, Optional

import jinja2

import esphome.config_validation as cv
from esphome.const import CONF_PACKAGES

TreeItem = Union[dict, list, str]


class TemplateRenderingError(Exception):

    def __init__(self, msg: str, original_error: Exception) -> None:
        msg += ': ' + str(original_error)
        super().__init__(msg)
        self.original_error = original_error


def _merge_package(full_old, full_new):
    def merge(old, new):
        # pylint: disable=no-else-return
        if isinstance(new, dict):
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                res[k] = merge(old[k], v) if k in old else v
            return res
        elif isinstance(new, list):
            if not isinstance(old, list):
                return new
            return old + new

        return new

    return merge(full_old, full_new)


def _has_jinja2_tags(value) -> bool:
    return '{{' in value or '{%' in value


def _create_template_for_str(str_val: str) -> jinja2.Template:
    # Configure template instance with required filters, extensions, blocks here
    return jinja2.Template(str_val)


def _process_template_value(value, context: dict):
    if not isinstance(value, str):
        return value  # Skip non-string values
    if not _has_jinja2_tags(value):
        return value  # If there are no Jinja2 tags - just skip it too
    try:
        template = _create_template_for_str(value)
        return template.render(context)
    except jinja2.TemplateError as e:
        raise TemplateRenderingError('Error in expression value \"{}\"'.format(value), e)


def _render_templates_for_item(val: TreeItem, context: dict, path: list) -> Optional[TreeItem]:
    # Walk the three recursively
    if isinstance(val, dict):
        for k, v in val.items():
            res = _render_templates_for_item(v, context, path + [k])
            if res is not None:
                val[k] = res
    elif isinstance(val, list):
        for i, it in enumerate(val):
            res = _render_templates_for_item(it, context, path + [i])
            if res is not None:
                val[i] = res
    elif isinstance(val, str):
        try:
            return _process_template_value(val, context)
        except TemplateRenderingError as e:
            raise cv.Invalid(str(e), path)

    return None


def do_template_rendering_pass(val: dict, context: dict):
    _render_templates_for_item(val, context, [])


def do_packages_pass(config: dict):
    """
    Packages syntax:
    Short (the old one):
        packages:
            pkg_name: !include folder/file_name.yaml
    Full (the new one):
        packages:
            pkg_name:
                source: !include folder/file_name.yaml
                variables:
                    var1: 1234
                    var2: abc
    :param config:
    :return:
    """
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    with cv.prepend_path(CONF_PACKAGES):
        if not isinstance(packages, dict):
            raise cv.Invalid("Packages must be a key to value mapping, got {} instead"
                             "".format(type(packages)))

        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                recursive_package = package_config
                if isinstance(package_config, dict):
                    recursive_package = do_packages_pass(package_config)
                processed_package = copy.deepcopy(recursive_package)
                do_template_rendering_pass(processed_package, {})
                config = _merge_package(processed_package, config)

        del config[CONF_PACKAGES]
    return config
