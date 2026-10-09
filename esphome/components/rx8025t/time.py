from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@remcom"]
DEPENDENCIES = ["i2c"]

rx8025t_ns = cg.esphome_ns.namespace("rx8025t")
RX8025TComponent = rx8025t_ns.class_(
    "RX8025TComponent", time.RealTimeClock, i2c.I2CDevice
)
CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RX8025TComponent),
    }
).extend(i2c.i2c_device_schema(0x32))


for _name, _call in (
    ("rx8025t.write_time", "write_time()"),
    ("rx8025t.read_time", "read_time()"),
):
    automation.register_apply_action(
        _name,
        automation.maybe_simple_id(
            {
                cv.GenerateID(): cv.use_id(RX8025TComponent),
            }
        ),
        automation.ApplyCall(_call),
    )


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
