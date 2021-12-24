import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_VALUE

CONF_SCALE = "scale"
CONF_ECC = "ecc"

CODEOWNERS = ["@wjtje"]

DEPENDENCIES = ["display"]
MULTI_CONF = True

qr_code_ns = cg.esphome_ns.namespace("qr_code")
QRCode = qr_code_ns.class_("QrCode", cg.Component)

Ecc = qr_code_ns.enum("Ecc")
ECC = {
    "LOW": Ecc.ECC_LOW,
    "MEDIUM": Ecc.ECC_MEDIUM,
    "QUARTILE": Ecc.ECC_QUARTILE,
    "HIGH": Ecc.ECC_HIGH,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(QRCode),
        cv.Required(CONF_VALUE): cv.string,
        cv.Optional(CONF_SCALE, default=1): cv.int_range(min=1),
        cv.Optional(CONF_ECC, default="LOW"): cv.enum(ECC, upper=True),
    }
)


async def to_code(config):
    cg.add_library("https://github.com/wjtje/QR-Code-generator-esphome", "1.7.0")
    cg.add_build_flag("-fexceptions")
    cg.add_platformio_option("build_unflags", "-fno-exceptions")

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_scale(config[CONF_SCALE]))
    cg.add(var.set_ecc(ECC[config[CONF_ECC]]))
    await cg.register_component(var, config)

    cg.add_define("USE_QR_CODE")
