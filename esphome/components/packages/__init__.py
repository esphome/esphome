import logging
from pathlib import Path

from esphome import git, yaml_util
from esphome.config_helpers import merge_config
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
    CONF_URL,
    CONF_USERNAME,
    CONF_VARS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_PACKAGES


def valid_package_contents(package_config: dict):
    """Validates that a package_config that will be merged looks as much as possible to a valid config
    to fail early on obvious mistakes."""
    if isinstance(package_config, dict):
        if CONF_URL in package_config:
            # If a URL key is found, then make sure the config conforms to a remote package schema:
            return REMOTE_PACKAGE_SCHEMA(package_config)

        # Validate manually since Voluptuous would regenerate dicts and lose metadata
        # such as ESPHomeDataBase
        for k, v in package_config.items():
            if not isinstance(k, str):
                raise cv.Invalid("Package content keys must be strings")
            if isinstance(v, (dict, list)):
                continue  # e.g. script: [] or logger: {level: debug}
            if v is None:
                continue  # e.g. web_server:
            raise cv.Invalid("Invalid component content in package definition")
        return package_config

    raise cv.Invalid("Package contents must be a dict")


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
        "Including a single package under `packages:` is deprecated. Use a list instead."
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


def _process_package(package_config, config, skip_update: bool = False):
    recursive_package = package_config
    if CONF_URL in package_config:
        package_config = _process_remote_package(package_config, skip_update)
    if isinstance(package_config, dict):
        recursive_package = do_packages_pass(package_config, skip_update)
    return merge_config(recursive_package, config)


def do_packages_pass(config: dict, skip_update: bool = False):
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    with cv.prepend_path(CONF_PACKAGES):
        packages = CONFIG_SCHEMA(packages)
        if isinstance(packages, dict):
            for package_name, package_config in reversed(packages.items()):
                with cv.prepend_path(package_name):
                    config = _process_package(package_config, config, skip_update)
        elif isinstance(packages, list):
            for package_config in reversed(packages):
                config = _process_package(package_config, config, skip_update)
        else:
            raise cv.Invalid(
                f"Packages must be a key to value mapping or list, got {type(packages)} instead"
            )

        del config[CONF_PACKAGES]
    return config
