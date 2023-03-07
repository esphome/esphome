import base64
import secrets
from pathlib import Path
from typing import Optional

import requests

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import git
from esphome.components.packages import validate_source_shorthand
from esphome.const import CONF_REF, CONF_WIFI
from esphome.wizard import wizard_file
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

    if project_name == "esphome.web":
        if "esp32c3" in import_url:
            board = "esp32-c3-devkitm-1"
            platform = "ESP32"
        elif "esp32s2" in import_url:
            board = "esp32-s2-saola-1"
            platform = "ESP32"
        elif "esp32s3" in import_url:
            board = "esp32-s3-devkitc-1"
            platform = "ESP32"
        elif "esp32" in import_url:
            board = "esp32dev"
            platform = "ESP32"
        elif "esp8266" in import_url:
            board = "esp01_1m"
            platform = "ESP8266"
        elif "pico-w" in import_url:
            board = "pico-w"
            platform = "RP2040"

        kwargs = {
            "name": name,
            "friendly_name": friendly_name,
            "platform": platform,
            "board": board,
            "ssid": "!secret wifi_ssid",
            "psk": "!secret wifi_password",
        }
        if encryption:
            noise_psk = secrets.token_bytes(32)
            key = base64.b64encode(noise_psk).decode()
            kwargs["api_encryption_key"] = key

        p.write_text(
            wizard_file(**kwargs),
            encoding="utf8",
        )
    else:
        git_file = git.GitFile.from_shorthand(import_url)

        if git_file.query and "full_config" in git_file.query:
            url = git_file.raw_url
            try:
                req = requests.get(url, timeout=30)
                req.raise_for_status()
            except requests.exceptions.RequestException as e:
                raise ValueError(f"Error while fetching {url}: {e}") from e

            p.write_text(req.text, encoding="utf8")

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
