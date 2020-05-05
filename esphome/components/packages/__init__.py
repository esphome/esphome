import esphome.config_validation as cv
from deepmerge import conservative_merger as package_merger

from esphome.const import CONF_PACKAGES


def _merge_package(config, package_name, package_config):
    config = config.copy()
    package_merger.merge(config, package_config)
    return config


def do_packages_pass(config):
    if CONF_PACKAGES not in config:
        return
    packages = config[CONF_PACKAGES]
    temp_config = config.copy()
    with cv.prepend_path(CONF_PACKAGES):
        if not isinstance(packages, dict):
            raise cv.Invalid("Packages must be a key to value mapping, got {} instead"
                             "".format(type(packages)))
        del temp_config[CONF_PACKAGES]
        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                if not isinstance(package_config, dict):
                    raise cv.Invalid("Package definition must be a dictionary containing valid esphome configuration "
                                     "to be merged with the main config, got {} instead"
                                     "".format(type(package_config)))
                temp_config = _merge_package(temp_config, package_name, package_config)
        config.clear()
        config.update(temp_config)
