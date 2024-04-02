import esphome.config_validation as cv

from esphome.const import CONF_ESPHOME, CONF_OTA, CONF_PLATFORM

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network"]

IS_PLATFORM_COMPONENT = True


def _ota_final_validate(config):
    if len(config) < 1:
        raise cv.Invalid(
            f"At least one platform must be specified for '{CONF_OTA}'; add '{CONF_PLATFORM}: {CONF_ESPHOME}' for original OTA functionality"
        )


FINAL_VALIDATE_SCHEMA = _ota_final_validate
