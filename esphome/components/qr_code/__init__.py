import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_VALUE
from esphome.types import ConfigType

CONF_SCALE = "scale"
CONF_ECC = "ecc"

CODEOWNERS = ["@wjtje"]
DOMAIN = "qr_code"

DEPENDENCIES = ["display"]
MULTI_CONF = True

qr_code_ns = cg.esphome_ns.namespace("qr_code")
QRCode = qr_code_ns.class_("QrCode", cg.Component)

qrcodegen_Ecc = cg.esphome_ns.enum("qrcodegen_Ecc")
ECC = qrcodegen_Ecc.options(
    "LOW", "MEDIUM", "QUARTILE", "HIGH", prefix="qrcodegen_Ecc_"
)

CONFIG_SCHEMA = cv.ensure_list(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(QRCode),
            cv.Required(CONF_VALUE): cv.string,
            cv.Optional(CONF_ECC, default="LOW"): cv.enum(ECC, upper=True),
        }
    )
)


async def to_code(config: list[ConfigType]) -> None:
    cg.add_library("wjtje/qr-code-generator-library", "1.7.0")

    for entry in config:
        var = cg.new_Pvariable(entry[CONF_ID])
        cg.add(var.set_value(entry[CONF_VALUE]))
        cg.add(var.set_ecc(ECC[entry[CONF_ECC]]))
        await cg.register_component(var, entry)

    cg.add_define("USE_QR_CODE")
