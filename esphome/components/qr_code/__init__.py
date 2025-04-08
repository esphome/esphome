import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_VALUE

CONF_SCALE = "scale"
CONF_ECC = "ecc"

CODEOWNERS = ["@wjtje"]

DEPENDENCIES = ["display"]
MULTI_CONF = True

qr_code_ns = cg.esphome_ns.namespace("qr_code")
QRCode = qr_code_ns.class_("QrCode", cg.Component)

qrcodegen_Ecc = cg.esphome_ns.enum("qrcodegen_Ecc")
ECC = {
    "LOW": qrcodegen_Ecc.qrcodegen_Ecc_LOW,
    "MEDIUM": qrcodegen_Ecc.qrcodegen_Ecc_MEDIUM,
    "QUARTILE": qrcodegen_Ecc.qrcodegen_Ecc_QUARTILE,
    "HIGH": qrcodegen_Ecc.qrcodegen_Ecc_HIGH,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(QRCode),
        cv.Required(CONF_VALUE): cv.string,
        cv.Optional(CONF_ECC, default="LOW"): cv.enum(ECC, upper=True),
    }
)


async def to_code(config):
    cg.add_library("wjtje/qr-code-generator-library", "^1.7.0")

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_value(config[CONF_VALUE]))
    cg.add(var.set_ecc(ECC[config[CONF_ECC]]))
    await cg.register_component(var, config)

    cg.add_define("USE_QR_CODE")
