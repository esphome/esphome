from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_CS_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_READ_PIN,
    CONF_WRITE_PIN,
)

tm1621_ns = cg.esphome_ns.namespace("tm1621")
TM1621Display = tm1621_ns.class_("TM1621Display", cg.PollingComponent)
TM1621DisplayRef = TM1621Display.operator("ref")

CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1621Display),
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_READ_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_WRITE_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs))
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
    read = await cg.gpio_pin_expression(config[CONF_READ_PIN])
    cg.add(var.set_read_pin(read))
    write = await cg.gpio_pin_expression(config[CONF_WRITE_PIN])
    cg.add(var.set_write_pin(write))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(TM1621DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
