import copy
import logging
import re
from typing import Optional, Dict

import esphome.config_validation as cv
from esphome.const import CONF_PACKAGES
from . import source_loaders
from .common import PackageDefinition, TreeItem, PackageConfig, PackageSource
from .expressions import process_expression_value, TemplateRenderingError

CODEOWNERS = ['@corvis', '@esphome/core']
_LOGGER = logging.getLogger(__name__)

# Config
CONF_SOURCE = 'source'
CONF_PARAMS = 'params'

VALID_CONTEXT_VAR_RE = re.compile(r'^[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z_][a-zA-Z0-9_]*)*$')

PACKAGE_SOURCE_LOADERS: Dict[str, source_loaders.BaseSourceLoader] = {
    'local': source_loaders.LocalSourceLoader()
}

PACKAGE_SOURCE_DEFAULT_HANDLER = 'local'

# Context
CONTEXT_PKG = 'pkg'
CONTEXT_PKG_PARAMS = 'params'


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


def _create_context_for_package(package: PackageDefinition) -> dict:
    return {
        CONTEXT_PKG: {
            CONTEXT_PKG_PARAMS: package.params
        }
    }


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
            return process_expression_value(val, context)
        except TemplateRenderingError as e:
            raise cv.Invalid(str(e), path)

    return None


def _is_short_package_config_syntax(package_config: PackageConfig) -> bool:
    if isinstance(package_config, str):
        return True
    if isinstance(package_config, dict):
        return CONF_SOURCE not in package_config.keys()
    return False


def _load_package_source(package_source: PackageSource, package_def: PackageDefinition):
    if isinstance(package_source, dict):
        package_def.content = copy.deepcopy(package_source)
        _LOGGER.warning("You are using outdated syntax for package \"%s\" definition. "
                        "Consider specifying package with string locator e.g. "
                        "\"local:path/to/package.yaml\" (path is relative to the current "
                        "yaml file)", package_def.local_name)
        return

    if not isinstance(package_source, str):
        raise cv.Invalid('Package source is incorrect. Expected source definition is either'
                         'a string locator or dictionary containing package configuration. '
                         )
    handler: source_loaders.BaseSourceLoader = None
    locator: str = None
    for prefix in PACKAGE_SOURCE_LOADERS:
        if package_source.startswith(prefix + ':'):
            handler = PACKAGE_SOURCE_LOADERS[prefix]
            locator = package_source
            break
    if handler is None:
        if ':' in package_source:
            raise cv.Invalid('Unknown package loader \"{loader}\" for package {package}'
                             .format(loader=package_source.split(':', 1)[0],
                                     package=package_def.local_name))
        # If there is no prefix - default locator should be used
        locator = ':'.join((PACKAGE_SOURCE_DEFAULT_HANDLER, package_source))
        handler = PACKAGE_SOURCE_LOADERS[PACKAGE_SOURCE_DEFAULT_HANDLER]
    handler.load(locator, package_def)
    assert package_def.content is not None, "Package loader {} didn't update content of " \
                                            "the package. By convention it should either " \
                                            "update content or raise error."


def _validate_package_config(package_name: str, package_config: PackageConfig):
    """
    There are 2 versions of the syntax supported.
    The short one: Either dictionary representing package source or string locator pointing
                   the package file or directory
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
                                         "must be valid python identifiers."
                                         .format(key), [CONF_PARAMS, key])


def _load_package(package_name, package_config: PackageConfig,
                  parent: Optional[PackageDefinition] = None) -> PackageDefinition:
    _validate_package_config(package_name, package_config)
    package = PackageDefinition(package_name, parent)
    if _is_short_package_config_syntax(package_config):
        _load_package_source(package_config, package)
    else:
        _load_package_source(package_config[CONF_SOURCE], package)
        package.params = package_config.get(CONF_PARAMS, {})
        if isinstance(package_config[CONF_SOURCE], str):
            package.external_ref = package_config[CONF_SOURCE]
    return package


def do_template_rendering_pass(val: dict, context: dict):
    _render_templates_for_item(val, context, [])


def do_packages_pass(config: dict, parent: PackageConfig = None):
    """
    Packages syntax:
    Short (the old one):
        packages:
            pkg_name: !include folder/file_name.yaml
    Full (the new one):
        packages:
            pkg_name:
                source: local:folder/file_name.yaml
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
                package = _load_package(package_name, package_config, parent)
                recursive_package_content = package.content
                if isinstance(package_config, dict):
                    recursive_package_content = do_packages_pass(recursive_package_content, package)
                do_template_rendering_pass(recursive_package_content,
                                           _create_context_for_package(package))
                config = _merge_package(recursive_package_content, config)

        del config[CONF_PACKAGES]
    return config
