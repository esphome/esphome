import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, wk2168, wk_base
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
)

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["wk2168", "wk_base"]
MULTI_CONF = True
CONF_WK2168 = "wk2168_spi"

wk2168_spi_ns = cg.esphome_ns.namespace("wk2168_spi")
WK2168ComponentSPI = wk2168_spi_ns.class_(
    "WK2168ComponentSPI", wk_base.WKBaseComponent, spi.SPIDevice
)
WK2168GPIOPinSPI = wk2168_spi_ns.class_(
    "WK2168GPIOPinSPI", cg.GPIOPin, cg.Parented.template(WK2168ComponentSPI)
)

CONFIG_SCHEMA = cv.All(
    wk_base.WKBASE_SCHEMA.extend(
        {cv.GenerateID(): cv.declare_id(WK2168ComponentSPI)}
    ).extend(spi.spi_device_schema()),
    wk2168.check_channel_wk2168,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_build_flag("-DI2C_BUFFER_LENGTH=255")
    cg.add(var.set_name(str(config[CONF_ID])))
    await wk_base.register_wk_base(var, config)
    await spi.register_spi_device(var, config)


WK2168_PIN_SCHEMA = cv.All(
    wk2168.WK2168_PIN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WK2168GPIOPinSPI),
            cv.Required(CONF_WK2168): cv.use_id(WK2168ComponentSPI),
        },
    ),
    wk2168.validate_pin_mode,
)


@pins.PIN_SCHEMA_REGISTRY.register("wk2168_spi", WK2168_PIN_SCHEMA)
async def sc16is75x_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_WK2168])
    cg.add(var.set_parent(parent))
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
