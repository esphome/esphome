from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

CODEOWNERS = ["@your-github-username"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_BME680_ID = "bme680_id"

bme680_ns = cg.esphome_ns.namespace("bme680")

BME680Oversampling = bme680_ns.enum("BME680Oversampling")
OVERSAMPLING_OPTIONS = {
    "NONE": BME680Oversampling.BME680_OVERSAMPLING_NONE,
    "1X": BME680Oversampling.BME680_OVERSAMPLING_1X,
    "2X": BME680Oversampling.BME680_OVERSAMPLING_2X,
    "4X": BME680Oversampling.BME680_OVERSAMPLING_4X,
    "8X": BME680Oversampling.BME680_OVERSAMPLING_8X,
    "16X": BME680Oversampling.BME680_OVERSAMPLING_16X,
}

BME680IIRFilter = bme680_ns.enum("BME680IIRFilter")
IIR_FILTER_OPTIONS = {
    "OFF": BME680IIRFilter.BME680_IIR_FILTER_OFF,
    "1X": BME680IIRFilter.BME680_IIR_FILTER_1X,
    "3X": BME680IIRFilter.BME680_IIR_FILTER_3X,
    "7X": BME680IIRFilter.BME680_IIR_FILTER_7X,
    "15X": BME680IIRFilter.BME680_IIR_FILTER_15X,
    "31X": BME680IIRFilter.BME680_IIR_FILTER_31X,
    "63X": BME680IIRFilter.BME680_IIR_FILTER_63X,
    "127X": BME680IIRFilter.BME680_IIR_FILTER_127X,
}

BME680Component = bme680_ns.class_(
    "BME680Component", cg.PollingComponent, i2c.I2CDevice
)

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
