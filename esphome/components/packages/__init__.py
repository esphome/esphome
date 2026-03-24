from collections import UserDict
from collections.abc import Callable
from functools import reduce
import logging
from pathlib import Path
from typing import Any

from esphome import git, yaml_util
from esphome.components.substitutions import ContextVars, push_context, substitute
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


def is_remote_package(package_config: dict) -> bool:
    """Returns True if the package_config is a remote package definition."""
    return CONF_URL in package_config


def valid_package_contents(package_config: dict) -> dict:
    """Checks if a package_config that will be merged looks as
    much as possible to a valid config to fail early on obvious mistakes.
    """

    if not isinstance(package_config, dict):
        raise cv.Invalid("Package contents must be a dict")

    if is_remote_package(package_config):
        # Package contents must not contain a root `url:` key
        raise cv.Invalid("Remote package schema not expected here")

    # Validate manually since Voluptuous would regenerate dicts and lose metadata
    # such as ESPHomeDataBase
    for k, v in package_config.items():
        if not isinstance(k, str):
            raise cv.Invalid("Package content keys must be strings")
        if isinstance(v, (dict, list, Remove)):
            continue  # e.g. script: [], psram: !remove, logger: {level: debug}
        if v is None:
            continue  # e.g. web_server:
        if isinstance(v, str) and has_jinja(v):
            # e.g: remote package shorthand:
            # package_name: github://esphome/repo/file.yaml@${ branch }, or:
            # switch: ${ expression that evals to a switch }
            continue

        raise cv.Invalid("Invalid component content in package definition")
    return package_config


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


def deprecate_single_package(config: dict) -> dict:
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
    valid_package_contents,  # Something that at least looks like an actual package, e.g. {wifi:{ssid: xxx}}
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

    def _load_package_yaml(yaml_file: Path, filename: str) -> dict:
        """Load a YAML file from a remote package, validating min_version."""
        try:
            new_yaml = yaml_util.load_yaml(yaml_file)
        except EsphomeError as e:
            raise cv.Invalid(
                f"{filename} is not a valid YAML file."
                f" Please check the file contents.\n{e}"
            ) from e
        esphome_config = new_yaml.get(CONF_ESPHOME) or {}
        min_version = esphome_config.get(CONF_MIN_VERSION)
        if min_version is not None and cv.Version.parse(min_version) > cv.Version.parse(
            ESPHOME_VERSION
        ):
            raise cv.Invalid(
                f"Current ESPHome Version is too old to use"
                f" this package: {ESPHOME_VERSION} < {min_version}"
            )
        return new_yaml

    def get_packages(files: list[dict[str, Any]]) -> dict:
        packages = {}
        for idx, file in enumerate(files):
            filename = file[CONF_PATH]
            yaml_file: Path = repo_dir / filename
            if not yaml_file.is_file():
                raise cv.Invalid(
                    f"{filename} does not exist in repository",
                    path=[CONF_FILES, idx, CONF_PATH],
                )
            new_yaml = _load_package_yaml(yaml_file, filename)
            new_yaml = yaml_util.add_context(new_yaml, file.get(CONF_VARS))
            packages[f"{filename}{idx}"] = new_yaml
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
    config: dict,
    callback: Callable[[dict, Any], dict],
    context: Any = None,
    validate_deprecated: bool = True,
) -> dict:
    """Walks the packages structure in priority order, invoking ``callback`` on each package definition found.

    This function only iterates over the immediate ``packages:`` entries in *config*.
    If packages may contain nested ``packages:`` keys, the *callback* is responsible
    for recursing by calling ``_walk_packages`` on the returned package config.
    """
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]

    if not isinstance(packages, (dict, list)):
        raise cv.Invalid(
            f"Packages must be a key to value mapping or list, got {type(packages)} instead"
        )

    with cv.prepend_path(CONF_PACKAGES):
        if not isinstance(packages, dict):
            _walk_package_list(packages, callback, context)
        elif (result := _walk_package_dict(packages, callback, context)) is not None:
            if not validate_deprecated:
                raise result
            # Fallback: treat the dict as a single deprecated package.
            # This block can be removed once the single-package
            # deprecation period (2026.7.0) is over.
            config[CONF_PACKAGES] = [packages]
            return _walk_packages(deprecate_single_package(config), callback, context)

    config[CONF_PACKAGES] = packages
    return config


def _walk_package_dict(
    packages: dict,
    callback: Callable[[dict, Any], dict],
    context: Any,
) -> cv.Invalid | None:
    """Iterate a packages dict in reverse priority order, invoking callback on each entry.

    Returns ``None`` on success, or the first :class:`cv.Invalid` error if a callback fails.
    """
    for package_name, package_config in reversed(packages.items()):
        with cv.prepend_path(package_name):
            try:
                packages[package_name] = callback(package_config, context)
            except cv.Invalid as err:
                return err
    return None


def _walk_package_list(
    packages: list,
    callback: Callable[[dict, Any], dict],
    context: Any,
) -> None:
    """Iterate a packages list in reverse priority order, invoking callback on each entry."""
    for idx in reversed(range(len(packages))):
        with cv.prepend_path(idx):
            packages[idx] = callback(packages[idx], context)


def _substitute_package_definition(
    package_config: dict | str, context_vars: ContextVars
) -> dict:
    """If the package definition is a string (which may contain substitutions) or a remote package,
    attempt to substitute any variables in it, since these could affect the URL, file paths, refs, etc.
    This does not substitute inside the package contents itself, only the remote package definition fields
    because the remote package has not been even been downloaded yet."""

    if isinstance(package_config, str) or (
        isinstance(package_config, dict) and is_remote_package(package_config)
    ):
        package_config = substitute(
            item=package_config,
            path=[],
            parent_context=context_vars,
            strict_undefined=False,
        )
    return package_config


def _update_substitutions_context(
    parent_context: dict[str, Any],
    package_substitutions: dict[str, Any],
) -> None:
    """Resolve and add new substitutions to the parent context.

    Skips keys already present (higher-priority sources win).
    String values are substituted against the current context so that
    cross-references between substitutions are expanded when possible.
    """
    for key, value in package_substitutions.items():
        if key in parent_context:
            continue
        if not isinstance(value, str):
            parent_context[key] = value
            continue
        parent_context[key] = substitute(
            item=value,
            path=[CONF_SUBSTITUTIONS, key],
            parent_context=parent_context,
            strict_undefined=False,
        )


def do_packages_pass(
    config: dict,
    command_line_substitutions: dict[str, Any] | None = None,
    skip_update: bool = False,
) -> dict:
    """Processes, downloads and validates all packages in the config.
    Also extracts and merges all substitutions found in packages into the main config substitutions.
    """
    if CONF_PACKAGES not in config:
        return config

    # original substitutions, as extracted from packages:
    substitutions = UserDict(config.pop(CONF_SUBSTITUTIONS, {}))

    # parent context for substitutions in package definitions. This will be updated during
    # process_package_callback below as packages are explored and new substitutions are incorporated,
    # so package definitions with lower priority, i.e., declared earlier, may depend on substitutions coming
    # from packages with higher priority.
    parent_context = UserDict(command_line_substitutions or {})

    def process_package_callback(package_config: dict | str, context_vars: Any) -> dict:
        """This will be called for each package found in the config."""
        package_config = _substitute_package_definition(package_config, context_vars)
        package_config = PACKAGE_SCHEMA(package_config)

        if is_remote_package(package_config):
            package_config = _process_remote_package(package_config, skip_update)

        # Extract substitutions from the package and merge them into the main substitutions:
        package_substitutions = package_config.pop(CONF_SUBSTITUTIONS, {})
        if package_substitutions:
            substitutions.data = merge_config(package_substitutions, substitutions.data)
            _update_substitutions_context(parent_context, package_substitutions)

        if CONF_PACKAGES not in package_config:
            return package_config

        # Push context from !include vars on the package root and on the packages key itself
        context_vars = push_context(package_config, context_vars)
        context_vars = push_context(package_config[CONF_PACKAGES], context_vars)
        return _walk_packages(package_config, process_package_callback, context_vars)

    _update_substitutions_context(parent_context, substitutions)

    context_vars = push_context(config[CONF_PACKAGES], ContextVars(parent_context))
    _walk_packages(config, process_package_callback, context_vars)

    if substitutions:
        config[CONF_SUBSTITUTIONS] = substitutions.data

    return config


def merge_packages(config: dict) -> dict:
    """Merges all packages into the main config and removes the `packages:` key."""
    if CONF_PACKAGES not in config:
        return config

    # Build flat list of all package configs to merge in priority order:
    merge_list: list[dict] = []

    def process_package_callback(package_config: dict, context: Any) -> dict:
        """This will be called for each package found in the config."""
        merge_list.append(package_config)
        return _walk_packages(package_config, process_package_callback)

    _walk_packages(config, process_package_callback, validate_deprecated=False)
    # Merge all packages into the main config:
    config = reduce(lambda new, old: merge_config(old, new), merge_list, config)
    del config[CONF_PACKAGES]
    return config
