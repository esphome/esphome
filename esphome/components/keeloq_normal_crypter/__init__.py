import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

keeloq_normal_crypter_ns = cg.esphome_ns.namespace("keeloq_normal_crypter")
KeeloqNormalCrypter = keeloq_normal_crypter_ns.class_(
    "KeeloqNormalCrypter", cg.Component
)

CONF_MANUFACTURER_KEY = "manufacturer_key"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KeeloqNormalCrypter),
        cv.Optional(CONF_MANUFACTURER_KEY, default=0): cv.hex_uint64_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add(var.set_manufacturer_key(config[CONF_MANUFACTURER_KEY]))
