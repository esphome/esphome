"""External component registering a protocol that has no source file in remote_base."""

import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["remote_base"]

ns = cg.esphome_ns.namespace("fake_protocol")
FakeData = ns.struct("FakeData")
FakeTrigger = ns.class_("FakeTrigger", remote_base.RemoteReceiverTrigger)
FakeDumper = ns.class_("FakeDumper", remote_base.RemoteReceiverDumperBase)

CONFIG_SCHEMA = cv.Schema({})


@remote_base.register_trigger("fake", FakeTrigger, FakeData)
def fake_trigger(var: cg.MockObj, config: ConfigType) -> None:
    pass


@remote_base.register_dumper("fake", FakeDumper)
def fake_dumper(var: cg.MockObj, config: ConfigType) -> None:
    pass
