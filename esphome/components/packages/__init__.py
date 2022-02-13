import re
from pathlib import Path
from esphome.core import EsphomeError
from esphome.config_helpers import merge_config

from esphome import git, yaml_util
from esphome.const import (
    CONF_FILE,
    CONF_FILES,
    CONF_PACKAGES,
    CONF_REF,
    CONF_REFRESH,
    CONF_URL,
    CONF_USERNAME,
    CONF_PASSWORD,
)
import esphome.config_validation as cv

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

    m = re.match(
        r"github://([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)/([a-zA-Z0-9\-_.\./]+)(?:@([a-zA-Z0-9\-_.\./]+))?",
        value,
    )
    if m is None:
        raise cv.Invalid(
            "Source is not a file system path or in expected github://username/name/[sub-folder/]file-path.yml[@branch-or-tag] format!"
        )

    conf = {
        CONF_URL: f"https://github.com/{m.group(1)}/{m.group(2)}.git",
        CONF_FILE: m.group(3),
    }
    if m.group(4):
        conf[CONF_REF] = m.group(4)

    # print(conf)
    return BASE_SCHEMA(conf)


BASE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_URL): cv.url,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Exclusive(CONF_FILE, "files"): validate_yaml_filename,
            cv.Exclusive(CONF_FILES, "files"): cv.All(
                cv.ensure_list(validate_yaml_filename),
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
    repo_dir = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config[CONF_REFRESH],
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )
    files: str = config[CONF_FILES]

    packages = {}
    for file in files:
        yaml_file: Path = repo_dir / file

        if not yaml_file.is_file():
            raise cv.Invalid(f"{file} does not exist in repository", path=[CONF_FILES])

        try:
            packages[file] = yaml_util.load_yaml(yaml_file)
        except EsphomeError as e:
            raise cv.Invalid(
                f"{file} is not a valid YAML file. Please check the file contents."
            ) from e
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

        for package_name, package_config in packages.items():
            with cv.prepend_path(package_name):
                recursive_package = package_config
                if CONF_URL in package_config:
                    package_config = _process_base_package(package_config)
                if isinstance(package_config, dict):
                    recursive_package = do_packages_pass(package_config)
                config = merge_config(recursive_package, config)

        del config[CONF_PACKAGES]
    return config
