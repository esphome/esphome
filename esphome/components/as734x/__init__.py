import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_GAIN, CONF_ID, CONF_TYPE
from esphome.types import ConfigType

CODEOWNERS = ["@latonita", "@mrgnr"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
DOMAIN = "as734x"

CONF_AS734X_ID = "as734x_id"
CONF_ATIME = "atime"
CONF_ASTEP = "astep"

# Band order is a contract with the readout maps in as734x.cpp: the index selects the counts slot.
BANDS_43 = (
    "f1",
    "f2",
    "fz",
    "f3",
    "f4",
    "fy",
    "f5",
    "fxl",
    "f6",
    "f7",
    "f8",
    "nir",
    "clear",
)
BANDS_41 = tuple(b for b in BANDS_43 if b not in ("fz", "fy", "fxl"))

as734x_ns = cg.esphome_ns.namespace("as734x")
AS734XComponent = as734x_ns.class_(
    "AS734XComponent", cg.PollingComponent, i2c.I2CDevice
)

Gain = as734x_ns.enum("Gain")
GAINS_41 = {
    f"X{name}": getattr(Gain, f"GAIN_{name.replace('.', '_')}X")
    for name in ("0.5", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512")
}
GAINS_43 = {**GAINS_41, "X1024": Gain.GAIN_1024X, "X2048": Gain.GAIN_2048X}


def _validate_exposure(config: ConfigType) -> ConfigType:
    if config[CONF_ATIME] == 0 and config[CONF_ASTEP] == 0:
        raise cv.Invalid(f"{CONF_ATIME} and {CONF_ASTEP} must not both be 0")
    return config


def _schema(gains: dict, address: int) -> cv.Schema:
    return cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(AS734XComponent),
                cv.Optional(CONF_GAIN, default="X8"): cv.enum(gains),
                cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
                cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(address)),
        _validate_exposure,
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "AS7341": _schema(GAINS_41, 0x39),
        "AS7343": _schema(GAINS_43, 0x39),
        "TCS3448": _schema(GAINS_43, 0x59),
    },
    upper=True,
)


async def to_code(config: ConfigType) -> None:
    chip = config[CONF_TYPE]
    cg.add_define("USE_AS7341" if chip == "AS7341" else "USE_AS7343")
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(f"&as734x::{chip}_CHIP"))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))
