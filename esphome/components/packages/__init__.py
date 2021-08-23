from pathlib import Path

from esphome import git, yaml_util
from esphome.const import (
    CONF_FILE,
    CONF_PACKAGES,
    CONF_REF,
    CONF_REFRESH,
    CONF_URL,
)
import esphome.config_validation as cv

DOMAIN = CONF_PACKAGES


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


def validate_git_package(config: dict):
    for key, conf in config.items():
        if CONF_URL in conf:
            try:
                conf = BASE_SCHEMA(conf)
            except cv.Invalid as e:
                raise cv.Invalid(
                    "Extra keys not allowed in git based package", path=[key] + e.path
                ) from e
    return config


BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_URL): cv.url,
        cv.Required(CONF_FILE): cv.string,
        cv.Optional(CONF_REF): cv.git_ref,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            str: cv.Any(BASE_SCHEMA, dict),
        }
    ),
    validate_git_package,
)


def _process_base_package(config: dict) -> dict:
    repo_dir = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config[CONF_REFRESH],
        domain=DOMAIN,
    )

    yaml_file: Path = repo_dir / config[CONF_FILE]

    if not yaml_file.is_file():
        raise cv.Invalid("File does not exist in repository", path=[CONF_FILE])

    package_config = yaml_util.load_yaml(yaml_file)
    return package_config


def do_packages_pass(config: dict):
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    with cv.prepend_path(CONF_PACKAGES):
        packages = CONFIG_SCHEMA(packages)
        if not isinstance(packages, dict):
            raise cv.Invalid(
                "Packages must be a key to value mapping, got {} instead"
                "".format(type(packages))
            )

        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                recursive_package = package_config
                if CONF_URL in package_config:
                    package_config = _process_base_package(package_config)
                if isinstance(package_config, dict):
                    recursive_package = do_packages_pass(package_config)
                config = _merge_package(recursive_package, config)

        del config[CONF_PACKAGES]
    return config
