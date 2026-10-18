"""Encryption layer using XXTEA, with an optional rolling code."""

import hashlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import ENCRYPTION, PacketInterface, packet_interface_ns

CONF_ROLLING_CODE_ENABLE = "rolling_code_enable"

# A captured packet lets a guessed key be checked offline, so the key text is stretched to make each guess costly.
# Both ends derive the same key from the same text, so the salt is fixed; it keeps tables built for other uses of
# PBKDF2 from applying here.
KEY_SALT = b"esphome packet_interface xxtea"
KEY_ITERATIONS = 600_000

XxteaEncryption = packet_interface_ns.class_("XxteaEncryption", PacketInterface)

XXTEA_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(XxteaEncryption),
        cv.Required(CONF_KEY): cv.All(cv.sensitive(cv.string), cv.Length(min=1)),
        cv.Optional(CONF_ROLLING_CODE_ENABLE, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def derive_key(text: str) -> list[int]:
    """The 256-bit key for the key text, as 32-bit words in little endian order."""
    digest = hashlib.pbkdf2_hmac("sha256", text.encode(), KEY_SALT, KEY_ITERATIONS)
    return [int.from_bytes(digest[i : i + 4], "little") for i in range(0, 32, 4)]


# The rolling code keeps the last counter of one peer, and accepts a packet only once the peer has sent back this
# device's boot number
@ENCRYPTION.register(
    "xxtea",
    schema=XXTEA_SCHEMA,
    single_peer=lambda config: config[CONF_ROLLING_CODE_ENABLE],
    needs_session=lambda config: config[CONF_ROLLING_CODE_ENABLE],
    auto_load=lambda config: ["xxtea"],
)
async def build_xxtea(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_XXTEA")
    var = cg.new_Pvariable(config[CONF_ID], lower, config[CONF_ROLLING_CODE_ENABLE])
    cg.add(var.set_key(derive_key(config[CONF_KEY])))
    await cg.register_component(var, config)
    return var
