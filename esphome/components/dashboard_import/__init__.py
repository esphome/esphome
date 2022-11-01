from pathlib import Path
import re
import requests

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packages import validate_source_shorthand
from esphome.const import CONF_WIFI
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
    # ignore result, only check if it's a valid shorthand
    validate_source_shorthand(value)
    return value


CONF_PACKAGE_IMPORT_URL = "package_import_url"
CONF_IMPORT_FULL_CONFIG = "import_full_config"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PACKAGE_IMPORT_URL): validate_import_url,
        cv.Optional(CONF_IMPORT_FULL_CONFIG, default=False): cv.boolean,
    }
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
    cg.add(dashboard_import_ns.set_package_import_url(config[CONF_PACKAGE_IMPORT_URL]))


def import_config(
    path: str, name: str, project_name: str, import_url: str, network: str = CONF_WIFI
) -> None:
    p = Path(path)

    if p.exists():
        raise FileExistsError

    if project_name == "esphome.web":
        p.write_text(
            wizard_file(
                name=name,
                platform="ESP32" if "esp32" in import_url else "ESP8266",
                board="esp32dev" if "esp32" in import_url else "esp01_1m",
                ssid="!secret wifi_ssid",
                psk="!secret wifi_password",
            ),
            encoding="utf8",
        )
    else:
        m = re.match(
            r"github://([a-zA-Z0-9\-]+)/([a-zA-Z0-9\-\._]+)/([a-zA-Z0-9\-_.\./]+)(?:@([a-zA-Z0-9\-_.\./]+))(?:\?([a-zA-Z0-9\-_.\./]+))?",
            import_url,
        )

        if m is None:
            raise ValueError

        if m.group(4) and "full_config" in m.group(4):
            url = f"https://raw.githubusercontent.com/{m.group(1)}/{m.group(2)}/{m.group(4)}/{m.group(3)}"
            try:
                req = requests.get(url, timeout=30)
                req.raise_for_status()
            except requests.exceptions.RequestException as e:
                raise ValueError(f"Error while fetching {url}: {e}") from e

            p.write_text(req.text, encoding="utf8")

        else:
            config = {
                "substitutions": {"name": name},
                "packages": {project_name: import_url},
                "esphome": {
                    "name": "${name}",
                    "name_add_mac_suffix": False,
                },
            }
            output = dump(config)

            if network == CONF_WIFI:
                output += WIFI_CONFIG

            p.write_text(output, encoding="utf8")
