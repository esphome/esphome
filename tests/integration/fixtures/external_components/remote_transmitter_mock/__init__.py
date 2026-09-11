"""Host-only stand-in for remote_transmitter used by the IR/RF integration tests."""

import esphome.codegen as cg
from esphome.components import remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]
MULTI_CONF = True
AUTO_LOAD = ["remote_base"]

remote_transmitter_mock_ns = cg.esphome_ns.namespace("remote_transmitter_mock")
# Declared as a RemoteTransmitterComponent so the ir_rf_proxy platforms accept it in
# remote_transmitter_id; the C++ class derives from RemoteTransmitterBase, which is all
# those platforms use
MockRemoteTransmitter = remote_transmitter_mock_ns.class_(
    "MockRemoteTransmitter", remote_transmitter.RemoteTransmitterComponent
)

CONFIG_SCHEMA = cv.Schema(
    {cv.GenerateID(): cv.declare_id(MockRemoteTransmitter)}
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
