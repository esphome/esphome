"""Test only: a pico-sdk repeating timer calls defer() and set_timeout() from IRQ context while the main loop
does the same, like the host scheduler fixtures do from std::thread workers (RP2040/RP2350 only)."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "irq_sched_stress"
irq_sched_stress_ns = cg.esphome_ns.namespace("irq_sched_stress")
IrqSchedStress = irq_sched_stress_ns.class_("IrqSchedStress", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(IrqSchedStress)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
