from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

from . import CONF_BME680_ID, BME680Component

# CODEOWNERS = ["@your-github-username"]

MULTI_CONF = True

CONFIG_SCHEMA = i2c.i2c_device_schema(0x77).extend(
    {
        cv.GenerateID(CONF_BME680_ID): cv.use_id(BME680Component),
    }
)

FINAL_VALIDATE_SCHEMA = i2c.final_validate_device_schema(
    "bme680", require_sda=False, require_scl=False
)


@automation.register_action(
    "bme680.heater_off",
    cg.ParentedPrototype.Action(BME680Component),
    automation.Schema({}),
)
async def bme680_heater_off(_, parent, args):
    await parent.turn_off_heater()


async def to_code(config):
    # 添加 Bosch BME680 驅動庫依賴
    # https://github.com/BoschSensortec/BME680_driver
    cg.add_library("boschsensortec/BME680", "1.1.4")
