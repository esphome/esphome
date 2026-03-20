from collections import UserDict
from collections.abc import Callable
from functools import reduce
import logging
from pathlib import Path
from typing import Any

from esphome import git, yaml_util
from esphome.components.substitutions.jinja import has_jinja
from esphome.config_helpers import Remove, merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_FILE,
    CONF_FILES,
    CONF_MIN_VERSION,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_SUBSTITUTIONS,
    CONF_URL,
    CONF_USERNAME,
    CONF_VARS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_PACKAGES


def validate_has_jinja(value: Any):
    if not isinstance(value, str) or not has_jinja(value):
        raise cv.Invalid("string does not contain Jinja syntax")
    return value


def valid_package_contents(allow_jinja: bool = True) -> Callable[[Any], dict]:
    """Returns a validator that checks if a package_config that will be merged looks as
    much as possible to a valid config to fail early on obvious mistakes."""

    def validator(package_config: dict) -> dict:
        if isinstance(package_config, dict):
            if CONF_URL in package_config:
                # If a URL key is found, then make sure the config conforms to a remote package schema:
                return REMOTE_PACKAGE_SCHEMA(package_config)

            # Validate manually since Voluptuous would regenerate dicts and lose metadata
            # such as ESPHomeDataBase
            for k, v in package_config.items():
                if not isinstance(k, str):
                    raise cv.Invalid("Package content keys must be strings")
                if isinstance(v, (dict, list, Remove)):
                    continue  # e.g. script: [], psram: !remove, logger: {level: debug}
                if v is None:
                    continue  # e.g. web_server:
                if allow_jinja and isinstance(v, str) and has_jinja(v):
                    # e.g: remote package shorthand:
                    # package_name: github://esphome/repo/file.yaml@${ branch }, or:
                    # switch: ${ expression that evals to a switch }
                    continue

                raise cv.Invalid("Invalid component content in package definition")
            return package_config

        raise cv.Invalid("Package contents must be a dict")

    return validator


def expand_file_to_files(config: dict):
    if CONF_FILE in config:
        new_config = config
        new_config[CONF_FILES] = [config[CONF_FILE]]
        del new_config[CONF_FILE]
        return new_config
    return config


def validate_yaml_filename(value):
    value = cv.string(value)

    if not (value.endswith(".yaml") or value.endswith(".yml")):
        raise cv.Invalid("Only YAML (.yaml / .yml) files are supported.")

    return value


def validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Git URL shorthand only for strings")

    git_file = git.GitFile.from_shorthand(value)

    conf = {
        CONF_URL: git_file.git_url,
        CONF_FILE: git_file.filename,
    }
    if git_file.ref:
        conf[CONF_REF] = git_file.ref

    return REMOTE_PACKAGE_SCHEMA(conf)


def deprecate_single_package(config):
    _LOGGER.warning(
        """
        Including a single package under `packages:`, i.e., `packages: !include mypackage.yaml` is deprecated.
        This method for including packages will go away in 2026.7.0
        Please use a list instead:

        packages:
          - !include mypackage.yaml

        See https://github.com/esphome/esphome/pull/12116
        """
    )
    return config


REMOTE_PACKAGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_PATH): cv.string,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Exclusive(CONF_FILE, CONF_FILES): validate_yaml_filename,
            cv.Exclusive(CONF_FILES, CONF_FILES): cv.All(
                cv.ensure_list(
                    cv.Any(
                        validate_yaml_filename,
                        cv.Schema(
                            {
                                cv.Required(CONF_PATH): validate_yaml_filename,
                                cv.Optional(CONF_VARS, default={}): cv.Schema(
                                    {cv.string: object}
                                ),
                            }
                        ),
                    )
                ),
                cv.Length(min=1),
            ),
            cv.Optional(CONF_REF): cv.git_ref,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_FILE, CONF_FILES),
    expand_file_to_files,
)

PACKAGE_SCHEMA = cv.Any(  # A package definition is either:
    validate_source_shorthand,  # A git URL shorthand string that expands to a remote package schema, or
    REMOTE_PACKAGE_SCHEMA,  # a valid remote package schema, or
    validate_has_jinja,  # a Jinja string that may resolve to a package, or
    valid_package_contents(
        allow_jinja=True
    ),  # Something that at least looks like an actual package, e.g. {wifi:{ssid: xxx}}
    # which will have to be fully validated later as per each component's schema.
)

CONFIG_SCHEMA = cv.Any(  # under `packages:` we can have either:
    cv.Schema(
        {
            str: PACKAGE_SCHEMA,  # a named dict of package definitions, or
        }
    ),
    [PACKAGE_SCHEMA],  # a list of package definitions, or
    cv.All(  # a single package definition (deprecated)
        cv.ensure_list(PACKAGE_SCHEMA), deprecate_single_package
    ),
)


def _process_remote_package(config: dict, skip_update: bool = False) -> dict:
    # When skip_update is True, use NEVER_REFRESH to prevent updates
    actual_refresh = git.NEVER_REFRESH if skip_update else config[CONF_REFRESH]
    repo_dir, revert = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=actual_refresh,
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )
    files = []

    if base_path := config.get(CONF_PATH):
        repo_dir = repo_dir / base_path

    for file in config[CONF_FILES]:
        if isinstance(file, str):
            files.append({CONF_PATH: file, CONF_VARS: {}})
        else:
            files.append(file)

    def get_packages(files) -> dict:
        packages = {}
        for idx, file in enumerate(files):
            filename = file[CONF_PATH]
            yaml_file: Path = repo_dir / filename
            vars = file.get(CONF_VARS, {})

            if not yaml_file.is_file():
                raise cv.Invalid(
                    f"{filename} does not exist in repository",
                    path=[CONF_FILES, idx, CONF_PATH],
                )

            try:
                new_yaml = yaml_util.load_yaml(yaml_file)
                if (
                    CONF_ESPHOME in new_yaml
                    and CONF_MIN_VERSION in new_yaml[CONF_ESPHOME]
                ):
                    min_version = new_yaml[CONF_ESPHOME][CONF_MIN_VERSION]
                    if cv.Version.parse(min_version) > cv.Version.parse(
                        ESPHOME_VERSION
                    ):
                        raise cv.Invalid(
                            f"Current ESPHome Version is too old to use this package: {ESPHOME_VERSION} < {min_version}"
                        )
                new_yaml = yaml_util.substitute_vars(new_yaml, vars)
                packages[f"{filename}{idx}"] = new_yaml
            except EsphomeError as e:
                raise cv.Invalid(
                    f"{filename} is not a valid YAML file. Please check the file contents.\n{e}"
                ) from e
        return packages

    packages = None
    error = ""

    try:
        packages = get_packages(files)
    except cv.Invalid as e:
        error = e
        try:
            if revert is not None:
                revert()
                packages = get_packages(files)
        except cv.Invalid as er:
            error = er

    if packages is None:
        raise cv.Invalid(f"Failed to load packages. {error}", path=error.path)

    return {"packages": packages}


def _walk_packages(
    config: dict, callback: Callable[[dict], dict], validate_deprecated: bool = True
) -> dict:
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]

    # The following block and `validate_deprecated` parameter can be safely removed
    #  once single-package deprecation is effective
    if validate_deprecated:
        packages = CONFIG_SCHEMA(packages)

    with cv.prepend_path(CONF_PACKAGES):
        if isinstance(packages, dict):
            for package_name, package_config in reversed(packages.items()):
                with cv.prepend_path(package_name):
                    package_config = callback(package_config)
                    packages[package_name] = _walk_packages(package_config, callback)
        elif isinstance(packages, list):
            for idx in reversed(range(len(packages))):
                with cv.prepend_path(idx):
                    package_config = callback(packages[idx])
                    packages[idx] = _walk_packages(package_config, callback)
        else:
            raise cv.Invalid(
                f"Packages must be a key to value mapping or list, got {type(packages)} instead"
            )
    config[CONF_PACKAGES] = packages
    return config


def do_packages_pass(config: dict, skip_update: bool = False) -> dict:
    """Processes, downloads and validates all packages in the config.
    Also extracts and merges all substitutions found in packages into the main config substitutions.
    """
    if CONF_PACKAGES not in config:
        return config

    substitutions = UserDict(config.pop(CONF_SUBSTITUTIONS, {}))

    def process_package_callback(package_config: dict) -> dict:
        """This will be called for each package found in the config."""
        package_config = PACKAGE_SCHEMA(package_config)
        if isinstance(package_config, str):
            return package_config  # Jinja string, skip processing
        if CONF_URL in package_config:
            package_config = _process_remote_package(package_config, skip_update)
        # Extract substitutions from the package and merge them into the main substitutions:
        substitutions.data = merge_config(
            package_config.pop(CONF_SUBSTITUTIONS, {}), substitutions.data
        )
        return package_config

    _walk_packages(config, process_package_callback)

    if substitutions:
        config[CONF_SUBSTITUTIONS] = substitutions.data

    return config


def merge_packages(config: dict) -> dict:
    """Merges all packages into the main config and removes the `packages:` key."""
    if CONF_PACKAGES not in config:
        return config

    # Build flat list of all package configs to merge in priority order:
    merge_list: list[dict] = []

    validate_package = valid_package_contents(allow_jinja=False)

    def process_package_callback(package_config: dict) -> dict:
        """This will be called for each package found in the config."""
        merge_list.append(validate_package(package_config))
        return package_config

    _walk_packages(config, process_package_callback, validate_deprecated=False)
    # Merge all packages into the main config:
    config = reduce(lambda new, old: merge_config(old, new), merge_list, config)
    del config[CONF_PACKAGES]
    return config
