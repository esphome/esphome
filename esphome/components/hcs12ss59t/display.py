from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
    CONF_PAGES,
)

CONF_SCROLL_ENABLE = "scroll_enable"
CONF_SCROLL_SPEED = "scroll_speed"

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@Mechazawa"]

hcs12ss59t_ns = cg.esphome_ns.namespace("hcs12ss59t")
HCS12SS59TComponent = hcs12ss59t_ns.class_(
    "HCS12SS59TComponent", cg.PollingComponent, spi.SPIDevice
)
HCS12SS59TComponentRef = HCS12SS59TComponent.operator("ref")

CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HCS12SS59TComponent),
            cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
            cv.Optional(
                CONF_SCROLL_SPEED, default="300ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(spi.spi_device_schema())
    .extend(cv.polling_component_schema("500ms")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await display.register_display(var, config)

    cg.add(var.set_intensity(config[CONF_INTENSITY]))
    cg.add(var.set_scroll_speed(config[CONF_SCROLL_SPEED]))

    enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(HCS12SS59TComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
