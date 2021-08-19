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
    cv.Required(CONF_URL): cv.url,
    cv.Optional(CONF_REF): validate_git_ref,
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
        r"github://([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)(?:@([a-zA-Z0-9\-_.\./]+))?",
        value,
    )
    if m is None:
        raise cv.Invalid(
            "Source is not a file system path or in expected github://username/name[@branch-or-tag] format!"
        )
    conf = {
        CONF_TYPE: TYPE_GIT,
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
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, validate_refresh),
        cv.Optional(CONF_COMPONENTS, default="all"): cv.Any(
            "all", cv.ensure_list(cv.string)
        ),
    }
)


async def to_code(config):
    pass


def _compute_destination_path(key: str) -> Path:
    base_dir = Path(CORE.config_dir) / ".esphome" / DOMAIN
    h = hashlib.new("sha256")
    h.update(key.encode())
    return base_dir / h.hexdigest()[:8]


def _run_git_command(cmd, cwd=None):
    try:
        ret = subprocess.run(cmd, cwd=cwd, capture_output=True, check=False)
    except FileNotFoundError as err:
        raise cv.Invalid(
            "git is not installed but required for external_components.\n"
            "Please see https://git-scm.com/book/en/v2/Getting-Started-Installing-Git for installing git"
        ) from err

    if ret.returncode != 0 and ret.stderr:
        err_str = ret.stderr.decode("utf-8")
        lines = [x.strip() for x in err_str.splitlines()]
        if lines[-1].startswith("fatal:"):
            raise cv.Invalid(lines[-1][len("fatal: ") :])
        raise cv.Invalid(err_str)


def _process_git_config(config: dict, refresh) -> str:
    key = f"{config[CONF_URL]}@{config.get(CONF_REF)}"
    repo_dir = _compute_destination_path(key)
    if not repo_dir.is_dir():
        _LOGGER.info("Cloning %s", key)
        _LOGGER.debug("Location: %s", repo_dir)
        cmd = ["git", "clone", "--depth=1"]
        if CONF_REF in config:
            cmd += ["--branch", config[CONF_REF]]
        cmd += ["--", config[CONF_URL], str(repo_dir)]
        _run_git_command(cmd)

    else:
        # Check refresh needed
        file_timestamp = Path(repo_dir / ".git" / "FETCH_HEAD")
        # On first clone, FETCH_HEAD does not exists
        if not file_timestamp.exists():
            file_timestamp = Path(repo_dir / ".git" / "HEAD")
        age = datetime.datetime.now() - datetime.datetime.fromtimestamp(
            file_timestamp.stat().st_mtime
        )
        if age.total_seconds() > refresh.total_seconds:
            _LOGGER.info("Updating %s", key)
            _LOGGER.debug("Location: %s", repo_dir)
            # Stash local changes (if any)
            _run_git_command(
                ["git", "stash", "push", "--include-untracked"], str(repo_dir)
            )
            # Fetch remote ref
            cmd = ["git", "fetch", "--", "origin"]
            if CONF_REF in config:
                cmd.append(config[CONF_REF])
            _run_git_command(cmd, str(repo_dir))
            # Hard reset to FETCH_HEAD (short-lived git ref corresponding to most recent fetch)
            _run_git_command(["git", "reset", "--hard", "FETCH_HEAD"], str(repo_dir))

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
