import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import __version__

CODEOWNERS = ["@esphome/core"]

CONF_NEXT_URL = "next_url"

VALID_SUBSTITUTIONS = ["esphome_version", "ip_address", "device_name"]


def validate_next_url(value):
    value = cv.url(value)
    test = r"{{(?!" + r"\b|".join(VALID_SUBSTITUTIONS) + r"\b)(\w+)}}"
    result = re.search(test, value)
    if result:
        raise cv.Invalid(
            f"Invalid substitution(s) ({', '.join(result.groups())}) in next_url. Valid substitutions are: {', '.join(VALID_SUBSTITUTIONS)}"
        )
    return value


IMPROV_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NEXT_URL): validate_next_url,
    }
)


def _process_next_url(url: str):
    if "{{esphome_version}}" in url:
        url = url.replace("{{esphome_version}}", __version__)
    return url


async def setup_improv_core(var, config):
    if CONF_NEXT_URL in config:
        cg.add(var.set_next_url(_process_next_url(config[CONF_NEXT_URL])))
    cg.add_library("improv/Improv", "1.2.4")
