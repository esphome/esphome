"""Encryption layer using XXTEA, with an optional rolling code."""

import hashlib

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import ENCRYPTION, PacketInterface, packet_interface_ns

CONF_ROLLING_CODE_ENABLE = "rolling_code_enable"
CONF_ROLLING_CODE_MODE = "rolling_code_mode"
CONF_SAVE_PEER_BOOT = "save_peer_boot"

MODE_ECHO = "echo"
MODE_BROADCAST = "broadcast"

# A captured packet lets a guessed key be checked offline, so the key text is stretched to make each guess costly.
# Both ends derive the same key from the same text, so the salt is fixed; it keeps tables built for other uses of
# PBKDF2 from applying here.
KEY_SALT = b"esphome packet_interface xxtea"
KEY_ITERATIONS = 600_000

XxteaEncryption = packet_interface_ns.class_("XxteaEncryption", PacketInterface)
RollingCode = packet_interface_ns.enum("RollingCode", is_class=True)
ROLLING_CODE_MODES = {
    MODE_ECHO: RollingCode.ROLLING_CODE_ECHO,
    MODE_BROADCAST: RollingCode.ROLLING_CODE_BROADCAST,
}


def _rolling_code_mode(config: ConfigType) -> str | None:
    """The rolling code mode, or None if the rolling code is not enabled."""
    if not config[CONF_ROLLING_CODE_ENABLE]:
        return None
    return config.get(CONF_ROLLING_CODE_MODE, MODE_ECHO)


def _validate_rolling_code(config: ConfigType) -> ConfigType:
    mode = _rolling_code_mode(config)
    if mode is None and CONF_ROLLING_CODE_MODE in config:
        raise cv.Invalid(
            f"'{CONF_ROLLING_CODE_MODE}' needs '{CONF_ROLLING_CODE_ENABLE}: true'",
            path=[CONF_ROLLING_CODE_MODE],
        )
    if mode != MODE_BROADCAST and CONF_SAVE_PEER_BOOT in config:
        raise cv.Invalid(
            f"'{CONF_SAVE_PEER_BOOT}' is only used with '{CONF_ROLLING_CODE_MODE}: {MODE_BROADCAST}'",
            path=[CONF_SAVE_PEER_BOOT],
        )
    return config


XXTEA_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XxteaEncryption),
            cv.Required(CONF_KEY): cv.All(cv.sensitive(cv.string), cv.Length(min=1)),
            cv.Optional(CONF_ROLLING_CODE_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_ROLLING_CODE_MODE): cv.enum(
                ROLLING_CODE_MODES, lower=True
            ),
            cv.Optional(CONF_SAVE_PEER_BOOT): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(_validate_rolling_code)
)


def derive_key(text: str) -> list[int]:
    """The 256-bit key for the key text, as 32-bit words in little endian order."""
    digest = hashlib.pbkdf2_hmac("sha256", text.encode(), KEY_SALT, KEY_ITERATIONS)
    return [int.from_bytes(digest[i : i + 4], "little") for i in range(0, 32, 4)]


@ENCRYPTION.register(
    "xxtea",
    schema=XXTEA_SCHEMA,
    # The echo rolling code accepts a packet only once the peer has sent back this device's boot number
    needs_session=lambda config: _rolling_code_mode(config) == MODE_ECHO,
    auto_load=lambda config: ["xxtea"],
)
async def build_xxtea(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_XXTEA")
    mode = _rolling_code_mode(config)
    rolling_code = (
        RollingCode.ROLLING_CODE_NONE if mode is None else ROLLING_CODE_MODES[mode]
    )
    var = cg.new_Pvariable(config[CONF_ID], lower, rolling_code)
    cg.add(var.set_key(derive_key(config[CONF_KEY])))
    if mode == MODE_BROADCAST and config.get(CONF_SAVE_PEER_BOOT, True):
        cg.add(var.set_save_peer_boot(True))
    await cg.register_component(var, config)
    return var


automation.register_apply_action(
    "packet_interface.reset_rolling_code",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(XxteaEncryption)}),
    automation.ApplyCall("reset_peer()"),
)
