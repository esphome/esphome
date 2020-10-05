import esphome.config_validation as cv

from esphome.const import CONF_PACKAGES


def _merge_package(full_old, full_new):
    def find_and_merge(old_list, new_item):
        if isinstance(new_item, dict) and "id" in new_item:
            for n, item in enumerate(old_list):
                if isinstance(item, dict) and "id" in item and item["id"] == new_item["id"]:
                    old_list[n] = merge(item, new_item)
                    return True
        return False

    def merge_list(old, new):
        res = old.copy()
        for new_item in new:
            if not find_and_merge(res, new_item):
                res.append(new_item)
        return res

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
            return merge_list(old, new)

        return new

    return merge(full_old, full_new)


def do_packages_pass(config: dict):
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    with cv.prepend_path(CONF_PACKAGES):
        if not isinstance(packages, dict):
            raise cv.Invalid(
                "Packages must be a key to value mapping, got {} instead"
                "".format(type(packages))
            )

        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                recursive_package = package_config
                if isinstance(package_config, dict):
                    recursive_package = do_packages_pass(package_config)
                config = _merge_package(recursive_package, config)

        del config[CONF_PACKAGES]
    return config
