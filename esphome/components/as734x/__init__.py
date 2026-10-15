import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_GAIN, CONF_ID, CONF_TYPE, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

CODEOWNERS = ["@latonita", "@mrgnr"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]
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


# One integration cycle lasts (ATIME + 1) * (ASTEP + 1) * 2.78 us
CYCLE_STEP_SECONDS = 2.78e-6


def _validate_exposure(cycles: int):
    """Reject an exposure that is empty or longer than the update interval."""

    def validator(config: ConfigType) -> ConfigType:
        atime, astep = config[CONF_ATIME], config[CONF_ASTEP]
        if atime == 0 and astep == 0:
            raise cv.Invalid(f"{CONF_ATIME} and {CONF_ASTEP} must not both be 0")
        measurement = (atime + 1) * (astep + 1) * CYCLE_STEP_SECONDS * cycles
        if measurement > config[CONF_UPDATE_INTERVAL].total_seconds:
            raise cv.Invalid(
                f"A measurement takes {measurement:.1f}s with these {CONF_ATIME} and "
                f"{CONF_ASTEP} values, longer than the {CONF_UPDATE_INTERVAL}"
            )
        return config

    return validator


def _schema(gains: dict, address: int, cycles: int) -> cv.Schema:
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
        _validate_exposure(cycles),
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "AS7341": _schema(GAINS_41, 0x39, cycles=2),  # two SMUX steps per update
        "AS7343": _schema(GAINS_43, 0x39, cycles=3),
        "TCS3448": _schema(GAINS_43, 0x59, cycles=3),
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
