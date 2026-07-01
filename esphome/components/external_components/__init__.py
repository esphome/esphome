import logging
from pathlib import Path
import time
from typing import Any

import requests

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
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE, TimePeriodSeconds

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


async def to_code(config: dict[str, Any]) -> None:
    pass


def _process_git_config(config: dict[str, Any], refresh: TimePeriodSeconds) -> Path:
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


def _check_for_merged_prs(srcs: list[tuple]):
    cache_file = CORE.relative_internal_path(".merged_prs_cache")
    cache_file.parent.mkdir(parents=True, exist_ok=True)
    merged_prs = []
    if cache_file.is_file():
        with cache_file.open(encoding="utf-8") as f:
            merged_prs = f.read().splitlines()
        stale = (time.time() - cache_file.stat().st_mtime) > 3600
    else:
        cache_file.touch()
        stale = True
    check_numbers = [n for n, _ in srcs if n not in merged_prs]
    if stale and check_numbers:
        url = "https://pr-check.control-j.com"

        payload = {
            "release_tag": ESPHOME_VERSION,
            "pr_numbers": check_numbers,
            "repo_owner": "esphome",
            "repo_name": "esphome",
        }
        headers = {
            "Cache-Control": "no-cache, no-store, must-revalidate",
            "Pragma": "no-cache",
            "Expires": "0",
        }

        response = requests.post(url, json=payload, headers=headers, timeout=15)
        if response.status_code != 200:
            return
        result_data = response.json()
        new_merged_prs = [
            pr for pr, data in result_data.items() if data["status"] == "merged"
        ]
        if new_merged_prs:
            merged_prs.extend(new_merged_prs)
            with cache_file.open("w", encoding="utf-8") as f:
                f.write("\n".join(set(merged_prs)))
        else:
            cache_file.touch()
    for pr_number, components in srcs:
        if pr_number in merged_prs:
            # Use lazy % formatting to prevent unnecessary string concat if not logging
            _LOGGER.warning(
                "The git reference 'github://PR#%s' "
                "for components %s\n"
                "is a pull request that has been merged and released.\n"
                "You should remove the external_components configuration for this source.",
                pr_number,
                ", ".join(components),
            )


def _process_single_config(config: dict[str, Any]) -> None:
    conf = config[CONF_SOURCE]
    if conf[CONF_TYPE] == TYPE_GIT:
        with cv.prepend_path([CONF_SOURCE]):
            components_dir = _process_git_config(
                config[CONF_SOURCE], config[CONF_REFRESH]
            )

    elif conf[CONF_TYPE] == TYPE_LOCAL:
        components_dir = Path(CORE.relative_config_path(conf[CONF_PATH]))
    else:
        raise NotImplementedError

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


def do_external_components_pass(config: dict[str, Any]) -> None:
    conf = config.get(DOMAIN)
    if conf is None:
        return
    with cv.prepend_path(DOMAIN):
        conf = CONFIG_SCHEMA(conf)
        pr_srcs = []
        for i, c in enumerate(conf):
            with cv.prepend_path(i):
                _process_single_config(c)
                source = c[CONF_SOURCE]
                if (
                    source[CONF_TYPE] == TYPE_GIT
                    and "github.com/esphome/esphome" in source[CONF_URL]
                    and "pull" in source.get(CONF_REF, "")
                ):
                    pr_number = source[CONF_REF].split("/")[1]
                    pr_srcs.append((pr_number, c[CONF_COMPONENTS]))
        _check_for_merged_prs(pr_srcs)
