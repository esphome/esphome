import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_INDOOR,
    CONF_WATCHDOG_THRESHOLD,
    CONF_NOISE_LEVEL,
    CONF_SPIKE_REJECTION,
    CONF_LIGHTNING_THRESHOLD,
    CONF_MASK_DISTURBER,
    CONF_DIV_RATIO,
    CONF_CAPACITANCE,
)

AUTO_LOAD = ["sensor", "binary_sensor"]
MULTI_CONF = True

CONF_AS3935_ID = "as3935_id"

as3935_ns = cg.esphome_ns.namespace("as3935")
AS3935 = as3935_ns.class_("AS3935Component", cg.Component)

CONF_IRQ_PIN = "irq_pin"
AS3935_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AS3935),
        cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_INDOOR, default=True): cv.boolean,
        cv.Optional(CONF_NOISE_LEVEL, default=2): cv.int_range(min=1, max=7),
        cv.Optional(CONF_WATCHDOG_THRESHOLD, default=2): cv.int_range(min=1, max=10),
        cv.Optional(CONF_SPIKE_REJECTION, default=2): cv.int_range(min=1, max=11),
        cv.Optional(CONF_LIGHTNING_THRESHOLD, default=1): cv.one_of(
            1, 5, 9, 16, int=True
        ),
        cv.Optional(CONF_MASK_DISTURBER, default=False): cv.boolean,
        cv.Optional(CONF_DIV_RATIO, default=0): cv.one_of(0, 16, 32, 64, 128, int=True),
        cv.Optional(CONF_CAPACITANCE, default=0): cv.int_range(min=0, max=15),
    }
)


async def setup_as3935(var, config):
    await cg.register_component(var, config)

    irq_pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(irq_pin))
    cg.add(var.set_indoor(config[CONF_INDOOR]))
    cg.add(var.set_noise_level(config[CONF_NOISE_LEVEL]))
    cg.add(var.set_watchdog_threshold(config[CONF_WATCHDOG_THRESHOLD]))
    cg.add(var.set_spike_rejection(config[CONF_SPIKE_REJECTION]))
    cg.add(var.set_lightning_threshold(config[CONF_LIGHTNING_THRESHOLD]))
    cg.add(var.set_mask_disturber(config[CONF_MASK_DISTURBER]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    cg.add(var.set_capacitance(config[CONF_CAPACITANCE]))
