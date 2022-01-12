from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packages import validate_source_shorthand
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
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PACKAGE_IMPORT_URL): validate_import_url,
    }
)

WIFI_CONFIG = """

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
"""

ESPHOME_WEB_COMMON = (
    """# Enable logging
logger:

# Enable Home Assistant API
api:

# Allow OTA updates
ota:"""
    + WIFI_CONFIG
    + """  ap: {}

# In combination with the `ap` this allows the user
# to provision wifi credentials to the device via WiFi AP.
captive_portal:

web_server:
"""
)

ESPHOME_WEB_CONFIG_ESP32 = (
    """
esphome:
  name: "${name}"
  platform: ESP32
  board: esp32dev

"""
    + ESPHOME_WEB_COMMON
)

ESPHOME_WEB_CONFIG_ESP8266 = (
    """
esphome:
  name: "${name}"
  platform: ESP8266
  board: esp01_1m

"""
    + ESPHOME_WEB_COMMON
)


async def to_code(config):
    cg.add_define("USE_DASHBOARD_IMPORT")
    cg.add(dashboard_import_ns.set_package_import_url(config[CONF_PACKAGE_IMPORT_URL]))


def import_config(path: str, name: str, project_name: str, import_url: str) -> None:
    p = Path(path)

    if p.exists():
        raise FileExistsError

    if project_name == "esphome.web":
        p.write_text(
            f"substitutions:\n  name: {name}\n" + ESPHOME_WEB_CONFIG_ESP32
            if "esp32" in import_url
            else ESPHOME_WEB_CONFIG_ESP8266,
            encoding="utf8",
        )
    else:
        config = {
            "substitutions": {"name": name},
            "packages": {project_name: import_url},
            "esphome": {"name_add_mac_suffix": False},
        }
        p.write_text(
            dump(config) + WIFI_CONFIG,
            encoding="utf8",
        )
