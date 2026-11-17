"""Test-only helper: keeps pad input enabled on sniffed pins when a driver calls pin_mode()."""

import esphome.codegen as cg
import esphome.config_validation as cv

DOMAIN = "sniff"
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_build_flag("-Wl,--wrap=gpio_set_direction")
