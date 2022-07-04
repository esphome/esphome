from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fastled_bus
from esphome.components.fastled_bus import CONF_CHIP_CHANNELS
import esphome.components.fastled_spi.light as fastled_spi_light
from esphome.const import (
    CONF_CHIPSET,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_DATA_RATE,
    CONF_ID,
    CONF_NUM_CHIPS,
)

# AUTO_LOAD = ["fastled_bus"]
AUTO_LOAD = ["fastled_bus"]
CODEOWNERS = ["@mabels"]
MULTI_CONF = True


CONFIG_SCHEMA = cv.All(
    fastled_bus.CONFIG_BUS_SCHEMA.extend(
        {
            cv.GenerateID(CONF_ID): fastled_bus.FastledBusId,
            cv.Required(CONF_CHIPSET): cv.one_of(
                *fastled_spi_light.CHIPSETS, upper=True
            ),
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA_RATE): cv.frequency,
        }
    ),
    fastled_bus.REQUIRED_FRAMEWORK,
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_CHIP_CHANNELS], config[CONF_NUM_CHIPS]
    )
    fastled_bus.new_fastled_bus(var, config)
    data_rate = None
    if CONF_DATA_RATE in config:
        data_rate_khz = int(config[CONF_DATA_RATE] / 1000)
        if data_rate_khz < 1000:
            data_rate = cg.RawExpression(f"DATA_RATE_KHZ({data_rate_khz})")
        else:
            data_rate_mhz = int(data_rate_khz / 1000)
            data_rate = cg.RawExpression(f"DATA_RATE_MHZ({data_rate_mhz})")

    template_args = cg.TemplateArguments(
        cg.RawExpression(config[CONF_CHIPSET]),
        fastled_bus.rgb_order(config),
        config[CONF_DATA_PIN],
        config[CONF_CLOCK_PIN],
        data_rate,
    )
    cg.add(var.set_controller(fastled_bus.CLEDControllerFactory.create(template_args)))

    await cg.register_component(var, config)
