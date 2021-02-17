import copy
import re
from typing import Union, Optional, Dict, Any

import jinja2

import esphome.config_validation as cv
from esphome.const import CONF_PACKAGES

CODEOWNERS = ['@corvis', '@esphome/core']

# Typings
TreeItem = Union[dict, list, str]
PackageConfig = Union[dict, str]
PackageSource = Union[dict, str]
PackageParams = Dict[str, Any]

# Config
CONF_SOURCE = 'source'
CONF_PARAMS = 'params'

VALID_CONTEXT_VAR_RE = re.compile('^[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z_][a-zA-Z0-9_]*)*$')

# Context
CONTEXT_PKG_PARAMS = 'pkg_params'


class PackageDefinition(object):
    """
    The class representing package instantiated with particular params
    """

    def __init__(self, local_name: str) -> None:
        self.local_name: str = local_name
        self.content: dict = None
        self.external_ref: str = None
        self.params: PackageParams = {}


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


def _create_context_for_package(package: PackageDefinition) -> dict:
    return {
        CONTEXT_PKG_PARAMS: package.params
    }


def _process_template_value(value, context: dict):
    if not isinstance(value, str):
        return value  # Skip non-string values
    if not _has_jinja2_tags(value):
        return value  # If there are no Jinja2 tags - just skip it too
    try:
        template = _create_template_for_str(value)
        return template.render(context)
    except jinja2.TemplateError as e:
        raise TemplateRenderingError(
            'Error in expression value \"{}\"'.format(value), e) from e


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


def _is_short_package_config_syntax(package_config: PackageConfig) -> bool:
    if isinstance(package_config, str):
        return True
    if isinstance(package_config, dict):
        return CONF_SOURCE not in package_config.keys()
    return False


def _load_package_source(package_source: PackageSource):
    if isinstance(package_source, dict):
        return copy.deepcopy(package_source)

    raise cv.Invalid('Package source is incorrect. Expected source definition is a dictionary '
                     'containing package configuration. You may also use include syntax '
                     'like this: !include pathto/my/file.yaml')
    # Validation on of the string uri should be added here


def _validate_package_config(package_name: str, package_config: PackageConfig):
    """
    There are 2 versions of the syntax supported.
    The short one: just dictionary representing package source.
                   In future it will be good to extend to support string URI pointing external
                   resource
    The regular one: dictionary of the following structure
                     source: dict or string - required, dictionary representing package source
                     params: dict           - optional, key-value params to be added to expression
                                                        evaluator scope for this package.
                                                        Value could be any type supported by YAML
    :param package_name:    name of the package to be validated
    :param package_config:  package configuration to be validated
    :return: None
    :raises: cv.Invalid - in case when configuration is invalid
    """
    if _is_short_package_config_syntax(package_config):
        # Nothing to validate short syntax means config is actually a source declaration
        # which will be validated during package loading phase
        pass
    else:
        if CONF_PARAMS in package_config:
            if not isinstance(package_config[CONF_PARAMS], dict):
                cv.Invalid('Package params should be key value mapping got {} instead'.format(
                    type(package_config[CONF_PARAMS])), [CONF_PARAMS])
            else:
                for key in package_config[CONF_PARAMS].keys():
                    if not VALID_CONTEXT_VAR_RE.fullmatch(key):
                        raise cv.Invalid("Invalid package parameter name {}. Parameter names "
                                         "must be valid python identifiers.".format(key), [CONF_PARAMS, key])


def _load_package(package_name, package_config: PackageConfig) -> PackageDefinition:
    _validate_package_config(package_name, package_config)
    package = PackageDefinition(package_name)
    if _is_short_package_config_syntax(package_config):
        package.content = _load_package_source(package_config)
    else:
        package.content = _load_package_source(package_config[CONF_SOURCE])
        package.params = package_config.get(CONF_PARAMS, {})
        if isinstance(package_config[CONF_SOURCE], str):
            package.external_ref = package_config[CONF_SOURCE]
    return package


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
                params:
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
                package = _load_package(package_name, package_config)
                recursive_package_content = package.content
                if isinstance(package_config, dict):
                    recursive_package_content = do_packages_pass(recursive_package_content)
                do_template_rendering_pass(recursive_package_content,
                                           _create_context_for_package(package))
                config = _merge_package(recursive_package_content, config)

        del config[CONF_PACKAGES]
    return config
