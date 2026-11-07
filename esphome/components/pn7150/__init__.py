import esphome.codegen as cg
from esphome.components import pn71xx
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["pn71xx"]
CODEOWNERS = ["@kbx81", "@jesserockz"]

pn7150_ns = cg.esphome_ns.namespace("pn7150")
PN7150 = pn7150_ns.class_("PN7150", pn71xx.PN71xx)

PN7150_SCHEMA = pn71xx.PN71XX_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PN7150),
    }
)

pn71xx.register_is_writing_condition("pn7150.is_writing", PN7150)


async def setup_pn7150(var: MockObj, config: ConfigType) -> None:
    await pn71xx.setup_pn71xx(var, config)
