import esphome.config_validation as cv

from esphome.const import CONF_PACKAGES


def _merge_package(full_old, full_new):

    def merge(old, new):
        if isinstance(new, dict):
            # Different types, just use new
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                if k in old:
                    res[k] = merge(old[k], v)
                else:
                    res[k] = v
            return res
        elif isinstance(new, list):
            # Different types, just use new
            if not isinstance(old, dict):
                return new
            return old + new
        else:
            return new

    return merge(full_old, full_new)


def do_packages_pass(config: dict):
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
                config = _merge_package(config, recursive_package)

        del config[CONF_PACKAGES]
    return config
