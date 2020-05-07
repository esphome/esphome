from deepmerge import conservative_merger as package_merger

import esphome.config_validation as cv

from esphome.const import CONF_PACKAGES

VALID_PACKAGE_NAME_CHARS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'


def _merge_package(config, package_name, package_config):
    config = config.copy()
    package_merger.merge(config, package_config)
    return config


def _is_valid_package_name(value: str) -> bool:
    if not value:
        return False
    if value[0].isdigit():
        return False
    try:
        cv.valid_name(value)
    except cv.Invalid:
        return False
    return True


def do_packages_pass(config: dict):
    if CONF_PACKAGES not in config:
        return
    packages = config[CONF_PACKAGES]
    temp_config = config.copy()
    with cv.prepend_path(CONF_PACKAGES):
        if packages is not None and not isinstance(packages, dict):
            raise cv.Invalid("Packages must be a key to value mapping, got {} instead"
                             "".format(type(packages)))
        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                if not isinstance(package_config, dict):
                    raise cv.Invalid("Package definition must be a dictionary containing valid "
                                     "esphome configuration to be merged with the main "
                                     "config, got {} instead"
                                     .format(type(package_config)))
                if not _is_valid_package_name(package_name):
                    raise cv.Invalid("Package name is invalid. Valid name should consist of "
                                     "letters, numbers and underscores. It shouldn't also "
                                     "start with number")
                temp_config = _merge_package(temp_config, package_name, package_config)
        del temp_config[CONF_PACKAGES]
        config.clear()
        config.update(temp_config)
