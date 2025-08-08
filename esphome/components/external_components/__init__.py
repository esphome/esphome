import logging
from pathlib import Path

from esphome import git, loader
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPONENTS,
    CONF_EXTERNAL_COMPONENTS,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_URL,
    CONF_USERNAME,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_EXTERNAL_COMPONENTS


CONFIG_SCHEMA = cv.ensure_list(
    {
        cv.Required(CONF_SOURCE): cv.SOURCE_SCHEMA,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
        cv.Optional(CONF_COMPONENTS, default="all"): cv.Any(
            "all", cv.ensure_list(cv.string)
        ),
    }
)


async def to_code(config):
    pass


def _process_git_config(config: dict, refresh) -> str:
    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=refresh,
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )

    if path := config.get(CONF_PATH):
        if (repo_dir / path).is_dir():
            components_dir = repo_dir / path
        else:
            raise cv.Invalid(
                "Could not find components folder for source. Please check the source contains a '"
                + path
                + "' folder"
            )
    elif (repo_dir / "esphome" / "components").is_dir():
        components_dir = repo_dir / "esphome" / "components"
    elif (repo_dir / "components").is_dir():
        components_dir = repo_dir / "components"
    else:
        raise cv.Invalid(
            "Could not find components folder for source. Please check the source contains a 'components' or 'esphome/components' folder"
        )

    return components_dir


def _process_single_config(config: dict):
    conf = config[CONF_SOURCE]
    if conf[CONF_TYPE] == TYPE_GIT:
        with cv.prepend_path([CONF_SOURCE]):
            components_dir = _process_git_config(
                config[CONF_SOURCE], config[CONF_REFRESH]
            )
    elif conf[CONF_TYPE] == TYPE_LOCAL:
        components_dir = Path(CORE.relative_config_path(conf[CONF_PATH]))
    else:
        raise NotImplementedError()

    if config[CONF_COMPONENTS] == "all":
        num_components = len(list(components_dir.glob("*/__init__.py")))
        if num_components > 100:
            # Prevent accidentally including all components from an esphome fork/branch
            # In this case force the user to manually specify which components they want to include
            raise cv.Invalid(
                "This source is an ESPHome fork or branch. Please manually specify the components you want to import using the 'components' key",
                [CONF_COMPONENTS],
            )
        allowed_components = None
    else:
        for i, name in enumerate(config[CONF_COMPONENTS]):
            expected = components_dir / name / "__init__.py"
            if not expected.is_file():
                raise cv.Invalid(
                    f"Could not find __init__.py file for component {name}. Please check the component is defined by this source (search path: {expected})",
                    [CONF_COMPONENTS, i],
                )
        allowed_components = config[CONF_COMPONENTS]

    loader.install_meta_finder(components_dir, allowed_components=allowed_components)


def do_external_components_pass(config: dict) -> None:
    conf = config.get(DOMAIN)
    if conf is None:
        return
    with cv.prepend_path(DOMAIN):
        conf = CONFIG_SCHEMA(conf)
        for i, c in enumerate(conf):
            with cv.prepend_path(i):
                _process_single_config(c)
