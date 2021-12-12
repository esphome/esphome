import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
    CONF_CLK_PIN,
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_STB_PIN,
    CONF_DIO_PIN,
)


# from esphome.core import Lambda;

# CONF_SWITCH1_ACTION = "switch1_action"

tm1638_ns = cg.esphome_ns.namespace("tm1638")
TM1638Component = tm1638_ns.class_("TM1638Component", cg.PollingComponent)
TM1638ComponentRef = TM1638Component.operator("ref")

# pin_with_input_and_output_support = cv.All(
#    pins.internal_gpio_pin_number({CONF_INPUT: True}),
#    pins.internal_gpio_pin_number({CONF_OUTPUT: True}),
# )

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TM1638Component),
            cv.Required(
                CONF_CLK_PIN
            ): pins.gpio_output_pin_schema,  ## do we want input and output here
            cv.Required(CONF_STB_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DIO_PIN): pins.gpio_output_pin_schema,
            # cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_INTENSITY, default=7): cv.int_range(min=0, max=7),
            # cv.Optional(CONF_REVERSE_ENABLE, default=False): cv.boolean,
            # cv.Optional(CONF_SWITCH1_ACTION): automation.validate_automation(
            #    single=True
            # ),
        }
    ).extend(cv.polling_component_schema("1s"))
    # .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)

    # cg.add(var.set_intensity(config[CONF_INTENSITY]))

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))

    dio = await cg.gpio_pin_expression(config[CONF_DIO_PIN])
    cg.add(var.set_dio_pin(dio))

    stb = await cg.gpio_pin_expression(config[CONF_STB_PIN])
    cg.add(var.set_stb_pin(stb))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(TM1638ComponentRef, "it")], return_type=cg.void
        )

    cg.add(var.set_writer(lambda_))

    # if CONF_SWITCH1_ACTION in config:
    #    await automation.build_automation(
    #        var.get_switch1_action_trigger(), [], config[CONF_SWITCH1_ACTION]
    #    )


#  if CONF_COOL_ACTION in config:
# await automation.build_automation(
#     var.get_cool_action_trigger(), [], config[CONF_COOL_ACTION]
# )
# cg.add(var.set_supports_cool(True))
