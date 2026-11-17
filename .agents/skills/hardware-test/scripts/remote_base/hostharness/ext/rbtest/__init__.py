"""Test only: pulls remote_base into a host build with the AEHA, Haier and Mirage sources."""

from esphome.components import remote_base
import esphome.config_validation as cv

AUTO_LOAD = ["remote_base"]
DOMAIN = "rbtest"
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    for name in ("aeha", "haier", "mirage"):
        remote_base.request_protocol(name)
