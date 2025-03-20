import base64
from pathlib import Path
import re
import secrets
from typing import Optional

import requests
from ruamel.yaml import YAML

from esphome import git
import esphome.codegen as cg
from esphome.components.packages import validate_source_shorthand
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_PROJECT, CONF_REF, CONF_WIFI
import esphome.final_validate as fv
from esphome.yaml_util import dump

dashboard_import_ns = cg.esphome_ns.namespace("dashboard_import")

# payload is in `esphomelib` mdns record, which only exists if api
# is enabled
DEPENDENCIES = ["api"]
CODEOWNERS = ["@esphome/core"]


def validate_import_url(value):
    value = cv.string_strict(value)
    value = cv.Length(max=255)(value)
    validate_source_shorthand(value)
    return value


def validate_full_url(config):
    if not config[CONF_IMPORT_FULL_CONFIG]:
        return config
    source = validate_source_shorthand(config[CONF_PACKAGE_IMPORT_URL])
    if CONF_REF not in source:
        raise cv.Invalid(
            "Must specify a ref (branch or tag) to import from when importing full config"
        )
    return config


CONF_PACKAGE_IMPORT_URL = "package_import_url"
CONF_IMPORT_FULL_CONFIG = "import_full_config"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PACKAGE_IMPORT_URL): validate_import_url,
            cv.Optional(CONF_IMPORT_FULL_CONFIG, default=False): cv.boolean,
        }
    ),
    validate_full_url,
)


def _final_validate(config):
    full_config = fv.full_config.get()[CONF_ESPHOME]
    if CONF_PROJECT not in full_config:
        raise cv.Invalid(
            "Dashboard import requires the `esphome` -> `project` information to be provided."
        )


FINAL_VALIDATE_SCHEMA = _final_validate

WIFI_CONFIG = """

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
"""


async def to_code(config):
    cg.add_define("USE_DASHBOARD_IMPORT")
    url = config[CONF_PACKAGE_IMPORT_URL]
    if config[CONF_IMPORT_FULL_CONFIG]:
        url += "?full_config"
    cg.add(dashboard_import_ns.set_package_import_url(url))


def import_config(
    path: str,
    name: str,
    friendly_name: Optional[str],
    project_name: str,
    import_url: str,
    network: str = CONF_WIFI,
    encryption: bool = False,
) -> None:
    p = Path(path)

    if p.exists():
        raise FileExistsError

    git_file = git.GitFile.from_shorthand(import_url)

    if git_file.query and "full_config" in git_file.query:
        url = git_file.raw_url
        try:
            req = requests.get(url, timeout=30)
            req.raise_for_status()
        except requests.exceptions.RequestException as e:
            raise ValueError(f"Error while fetching {url}: {e}") from e

        contents = req.text
        yaml = YAML()
        loaded_yaml = yaml.load(contents)
        if (
            "name_add_mac_suffix" in loaded_yaml["esphome"]
            and loaded_yaml["esphome"]["name_add_mac_suffix"]
        ):
            loaded_yaml["esphome"]["name_add_mac_suffix"] = False
            name_val = loaded_yaml["esphome"]["name"]
            sub_pattern = re.compile(r"\$\{?([a-zA-Z-_]+)\}?")
            if match := sub_pattern.match(name_val):
                name_sub = match.group(1)
                if name_sub in loaded_yaml["substitutions"]:
                    loaded_yaml["substitutions"][name_sub] = name
                else:
                    raise ValueError(
                        f"Name substitution {name_sub} not found in substitutions"
                    )
            else:
                loaded_yaml["esphome"]["name"] = name
            if friendly_name is not None:
                friendly_name_val = loaded_yaml["esphome"]["friendly_name"]
                if match := sub_pattern.match(friendly_name_val):
                    friendly_name_sub = match.group(1)
                    if friendly_name_sub in loaded_yaml["substitutions"]:
                        loaded_yaml["substitutions"][friendly_name_sub] = friendly_name
                    else:
                        raise ValueError(
                            f"Friendly name substitution {friendly_name_sub} not found in substitutions"
                        )
                else:
                    loaded_yaml["esphome"]["friendly_name"] = friendly_name

            with p.open("w", encoding="utf8") as f:
                yaml.dump(loaded_yaml, f)
        else:
            with p.open("w", encoding="utf8") as f:
                f.write(contents)

    else:
        substitutions = {"name": name}
        esphome_core = {"name": "${name}", "name_add_mac_suffix": False}
        if friendly_name:
            substitutions["friendly_name"] = friendly_name
            esphome_core["friendly_name"] = "${friendly_name}"
        config = {
            "substitutions": substitutions,
            "packages": {project_name: import_url},
            "esphome": esphome_core,
        }
        if encryption:
            noise_psk = secrets.token_bytes(32)
            key = base64.b64encode(noise_psk).decode()
            config["api"] = {"encryption": {"key": key}}

        output = dump(config)

        if network == CONF_WIFI:
            output += WIFI_CONFIG

        p.write_text(output, encoding="utf8")
