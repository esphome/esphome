"""Test only: pulls remote_base with every protocol source into a host build."""

from esphome.components import remote_base
import esphome.config_validation as cv

AUTO_LOAD = ["remote_base"]
DOMAIN = "rbtest"
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    for stem in remote_base._PROTOCOL_STEMS:
        remote_base.request_protocol(stem)
