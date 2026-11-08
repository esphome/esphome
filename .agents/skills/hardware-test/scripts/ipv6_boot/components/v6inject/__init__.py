"""Test only: wrap esp_netif_action_connected so the wrapper in v6probe.hpp can delay it."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType

DOMAIN = "v6inject"
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    cg.add_build_flag("-Wl,--wrap=esp_netif_action_connected")
    cg.add_define("V6_INJECT")
