import re
import logging
from pathlib import Path

import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPONENTS,
    CONF_REF,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_URL,
    CONF_TYPE,
    CONF_EXTERNAL_COMPONENTS,
    CONF_PATH,
    CONF_USERNAME,
    CONF_PASSWORD,
)
from esphome.core import CORE
from esphome import git, loader

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_EXTERNAL_COMPONENTS

TYPE_GIT = "git"
TYPE_LOCAL = "local"


GIT_SCHEMA = {
    cv.Required(CONF_URL): cv.url,
    cv.Optional(CONF_REF): cv.git_ref,
    cv.Optional(CONF_USERNAME): cv.string,
    cv.Optional(CONF_PASSWORD): cv.string,
}
LOCAL_SCHEMA = {
    cv.Required(CONF_PATH): cv.directory,
}


def validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Shorthand only for strings")
    try:
        return SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    except cv.Invalid:
        pass
    # Regex for GitHub repo name with optional branch/tag
    # Note: git allows other branch/tag names as well, but never seen them used before
    m = re.match(
        r"github://(?:([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)(?:@([a-zA-Z0-9\-_.\./]+))?|pr#([0-9]+))",
        value,
    )
    if m is None:
        raise cv.Invalid(
            "Source is not a file system path, in expected github://username/name[@branch-or-tag] or github://pr#1234 format!"
        )
    if m.group(4):
        conf = {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: "https://github.com/esphome/esphome.git",
            CONF_REF: f"pull/{m.group(4)}/head",
        }
    else:
        conf = {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: f"https://github.com/{m.group(1)}/{m.group(2)}.git",
        }
        if m.group(3):
            conf[CONF_REF] = m.group(3)

    return SOURCE_SCHEMA(conf)


SOURCE_SCHEMA = cv.Any(
    validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: cv.Schema(GIT_SCHEMA),
            TYPE_LOCAL: cv.Schema(LOCAL_SCHEMA),
        }
    ),
)


CONFIG_SCHEMA = cv.ensure_list(
    {
        cv.Required(CONF_SOURCE): SOURCE_SCHEMA,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
        cv.Optional(CONF_COMPONENTS, default="all"): cv.Any(
            "all", cv.ensure_list(cv.string)
        ),
    }
)


async def to_code(config):
    pass


def _process_git_config(config: dict, refresh) -> str:
    repo_dir = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=refresh,
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )

    if (repo_dir / "esphome" / "components").is_dir():
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
