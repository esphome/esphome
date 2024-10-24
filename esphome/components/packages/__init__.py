from pathlib import Path

from esphome import git, yaml_util
from esphome.config_helpers import merge_config
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_FILE,
    CONF_FILES,
    CONF_MIN_VERSION,
    CONF_NAME,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_REF,
    CONF_REFRESH,
    CONF_URL,
    CONF_USERNAME,
    CONF_VARS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import EsphomeError

DOMAIN = CONF_PACKAGES


def validate_git_package(config: dict):
    new_config = config
    for key, conf in config.items():
        if CONF_URL in conf:
            try:
                conf = BASE_SCHEMA(conf)
                if CONF_FILE in conf:
                    new_config[key][CONF_FILES] = [conf[CONF_FILE]]
                    del new_config[key][CONF_FILE]
            except cv.MultipleInvalid as e:
                with cv.prepend_path([key]):
                    raise e
            except cv.Invalid as e:
                raise cv.Invalid(
                    "Extra keys not allowed in git based package",
                    path=[key] + e.path,
                ) from e
    return new_config


def validate_yaml_filename(value):
    value = cv.string(value)

    if not (value.endswith(".yaml") or value.endswith(".yml")):
        raise cv.Invalid("Only YAML (.yaml / .yml) files are supported.")

    return value


def validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Shorthand only for strings")

    git_file = git.GitFile.from_shorthand(value)

    conf = {
        CONF_URL: git_file.git_url,
        CONF_FILE: git_file.filename,
    }
    if git_file.ref:
        conf[CONF_REF] = git_file.ref

    return BASE_SCHEMA(conf)


BASE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Exclusive(CONF_FILE, "files"): validate_yaml_filename,
            cv.Exclusive(CONF_FILES, "files"): cv.All(
                cv.ensure_list(
                    cv.Any(
                        validate_yaml_filename,
                        cv.Schema(
                            {
                                cv.Required(CONF_NAME): validate_yaml_filename,
                                cv.Optional(CONF_VARS, default={}): cv.Schema(
                                    {cv.string: cv.string}
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
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            str: cv.Any(validate_source_shorthand, BASE_SCHEMA, dict),
        }
    ),
    validate_git_package,
)


def _process_base_package(config: dict) -> dict:
    repo_dir, revert = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config[CONF_REFRESH],
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )
    files = []

    for file in config[CONF_FILES]:
        if isinstance(file, str):
            files.append({CONF_NAME: file, CONF_VARS: {}})
        else:
            files.append(file)

    def get_packages(files) -> dict:
        packages = {}
        for idx, file in enumerate(files):
            filename = file[CONF_NAME]
            yaml_file: Path = repo_dir / filename
            vars = file.get(CONF_VARS, {})

            if not yaml_file.is_file():
                raise cv.Invalid(
                    f"{filename} does not exist in repository",
                    path=[CONF_FILES],
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
                vars = {k: str(v) for k, v in vars.items()}
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
        raise cv.Invalid(f"Failed to load packages. {error}")

    return {"packages": packages}


def do_packages_pass(config: dict):
    if CONF_PACKAGES not in config:
        return config
    packages = config[CONF_PACKAGES]
    with cv.prepend_path(CONF_PACKAGES):
        packages = CONFIG_SCHEMA(packages)
        if not isinstance(packages, dict):
            raise cv.Invalid(
                f"Packages must be a key to value mapping, got {type(packages)} instead"
            )

        for package_name, package_config in reversed(packages.items()):
            with cv.prepend_path(package_name):
                recursive_package = package_config
                if CONF_URL in package_config:
                    package_config = _process_base_package(package_config)
                if isinstance(package_config, dict):
                    recursive_package = do_packages_pass(package_config)
                config = merge_config(recursive_package, config)

        del config[CONF_PACKAGES]
    return config
