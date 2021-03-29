import re
import logging
from pathlib import Path
import subprocess
import hashlib
import datetime

import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPONENTS,
    CONF_SOURCE,
    CONF_URL,
    CONF_TYPE,
    CONF_EXTERNAL_COMPONENTS,
    CONF_PATH,
)
from esphome.core import CORE
from esphome import loader

_LOGGER = logging.getLogger(__name__)

DOMAIN = CONF_EXTERNAL_COMPONENTS

TYPE_GIT = "git"
TYPE_LOCAL = "local"
CONF_REFRESH = "refresh"
CONF_REF = "ref"


def validate_git_ref(value):
    if re.match(r"[a-zA-Z0-9\-_.\./]+", value) is None:
        raise cv.Invalid("Not a valid git ref")
    return value


GIT_SCHEMA = {
    cv.Required(CONF_TYPE): TYPE_GIT,
    cv.Required(CONF_URL): cv.url,
    cv.Optional(CONF_REF): validate_git_ref,
}
LOCAL_SCHEMA = {
    cv.Required(CONF_TYPE): TYPE_LOCAL,
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
        r"([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)(?:@([a-zA-Z0-9\-_.\./]+))?", value
    )
    if m is None:
        raise cv.Invalid(
            "Source is not in expected username/name[@branch-or-tag] format!"
        )
    conf = {
        CONF_TYPE: "git",
        CONF_URL: f"https://github.com/{m.group(1)}/{m.group(2)}.git",
    }
    if m.group(3):
        conf[CONF_REF] = m.group(3)
    return SOURCE_SCHEMA(conf)


def validate_refresh(value: str):
    if value.lower() == "always":
        return validate_refresh("0s")
    if value.lower() == "never":
        return validate_refresh("1000y")
    return cv.positive_time_period_seconds(value)


SOURCE_SCHEMA = cv.Any(validate_source_shorthand, GIT_SCHEMA, LOCAL_SCHEMA)


CONFIG_SCHEMA = cv.ensure_list(
    {
        cv.Required(CONF_SOURCE): SOURCE_SCHEMA,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, validate_refresh),
        cv.Optional(CONF_COMPONENTS, default="all"): cv.Any(
            "all", cv.ensure_list(cv.string)
        ),
    }
)


def to_code(config):
    pass


def _compute_destination_path(key: str) -> Path:
    base_dir = Path(CORE.config_dir) / ".esphome" / DOMAIN
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def _process_single_config(config: dict):
    conf = config[CONF_SOURCE]
    if conf[CONF_TYPE] == TYPE_GIT:
        key = f"{conf[CONF_URL]}@{conf.get(CONF_REF)}"
        repo_dir = _compute_destination_path(key)
        if not repo_dir.is_dir():
            cmd = ["git", "clone", "--depth=1"]
            if CONF_REF in conf:
                cmd += ["--branch", conf[CONF_REF]]
            cmd += [conf[CONF_URL], str(repo_dir)]
            # TODO: Error handling
            subprocess.check_call(cmd)
        else:
            # Check refresh needed
            file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
            # On first clone, FETCH_HEAD does not exists
            if not file_timestamp.exists():
                file_timestamp = Path(repo_dir / ".git" / "HEAD")
            age = datetime.datetime.now() - datetime.datetime.fromtimestamp(
                file_timestamp.stat().st_mtime
            )
            if age.seconds > config[CONF_REFRESH].total_seconds:
                _LOGGER.info("Executing git pull %s", key)
                cmd = ["git", "pull"]
                subprocess.check_call(cmd, cwd=repo_dir)

        dest_dir = repo_dir
    elif conf[CONF_TYPE] == TYPE_LOCAL:
        dest_dir = Path(conf[CONF_PATH])
    else:
        raise NotImplementedError()

    if (dest_dir / "esphome" / "components").is_dir():
        components_dir = dest_dir / "esphome" / "components"
    elif (dest_dir / "components").is_dir():
        components_dir = dest_dir / "components"
    else:
        raise cv.Invalid(
            "Could not find components folder for source. Please check the source contains a 'components' or 'esphome/components' folder",
            [CONF_SOURCE],
        )

    if config[CONF_COMPONENTS] == "all":
        num_components = len(list(components_dir.glob("*/__init__.py")))
        if num_components > 100:
            # Prevent accidentally including all components from an esphome fork/branch
            # In this case force the user to manually specify which components they want to include
            raise cv.Invalid(
                "This source is an esphome fork or branch. Please manually specify the components you want to import using the 'components' key",
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
