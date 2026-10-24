"""A2DP speaker output for a nearby Bluetooth audio sink."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_PIN
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mbrudka"]
DEPENDENCIES = ["esp32_ble"]
AUTO_LOAD = ["a2dp_source", "ring_buffer"]
CONF_SLOT = "slot"
CONF_APPROVE = "approve"
CONF_REQUIRE_DISCOVERED = "require_discovered"
CONF_PAGE = "page"
CONF_ON_PREEMPT = "on_preempt"

a2dp_source_ns = cg.esphome_ns.namespace("a2dp_source")
A2DPSourceSpeaker = a2dp_source_ns.class_(
    "A2DPSourceSpeaker", cg.Component, speaker.Speaker
)


def _set_stream_limits(config: ConfigType) -> ConfigType:
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=2,
        max_channels=2,
        min_sample_rate=44100,
        max_sample_rate=44100,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(A2DPSourceSpeaker),
            cv.Optional(CONF_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SLOT, default=1): cv.int_range(min=1, max=8),
            cv.Optional(CONF_ON_PREEMPT): automation.validate_automation(single=True),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _set_stream_limits,
)


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if esp32.get_esp32_variant() != esp32.VARIANT_ESP32:
        raise cv.Invalid("A2DP source requires the original ESP32")
    if not full_config["esp32_ble"]["classic_bt"]:
        raise cv.Invalid("A2DP source requires esp32_ble.classic_bt: true")
    speakers = [
        item
        for item in full_config.get("speaker", [])
        if item.get("platform") == "a2dp_source"
    ]
    if len({item[CONF_SLOT] for item in speakers}) != len(speakers):
        raise cv.Invalid("A2DP source slot numbers must be unique")
    addresses = [str(item[CONF_ADDRESS]) for item in speakers if CONF_ADDRESS in item]
    if len(set(addresses)) != len(addresses):
        raise cv.Invalid("A2DP source addresses must be unique")


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_slot(config[CONF_SLOT]))
    if on_preempt := config.get(CONF_ON_PREEMPT):
        await automation.build_automation(var.get_preempt_trigger(), [], on_preempt)
    if CONF_ADDRESS in config:
        cg.add(var.set_address(str(config[CONF_ADDRESS])))
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_ENABLE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_USE_EXTERNAL_CODEC", False)
    cg.add_define("USE_A2DP_SOURCE")


SPEAKER_ACTION_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(A2DPSourceSpeaker)})

automation.register_apply_action(
    "a2dp_source.start_discovery",
    SPEAKER_ACTION_SCHEMA,
    automation.ApplyCall("start_discovery()"),
)
automation.register_apply_action(
    "a2dp_source.cancel_discovery",
    SPEAKER_ACTION_SCHEMA,
    automation.ApplyCall("cancel_discovery()"),
)
automation.register_apply_action(
    "a2dp_source.clear_selection",
    SPEAKER_ACTION_SCHEMA,
    automation.ApplyCall("clear_address()"),
)
automation.register_apply_action(
    "a2dp_source.unpair",
    SPEAKER_ACTION_SCHEMA,
    automation.ApplyCall("unpair()"),
)

ADDRESS_ACTION_SCHEMA = SPEAKER_ACTION_SCHEMA.extend(
    {cv.Required(CONF_ADDRESS): cv.templatable(cv.mac_address)}
)
automation.register_apply_action(
    "a2dp_source.select",
    ADDRESS_ACTION_SCHEMA,
    automation.ApplyCall("select_bond({}.c_str())", ((CONF_ADDRESS, cg.std_string),)),
)
automation.register_apply_action(
    "a2dp_source.forget",
    ADDRESS_ACTION_SCHEMA,
    automation.ApplyCall("forget_bond({}.c_str())", ((CONF_ADDRESS, cg.std_string),)),
)
automation.register_apply_action(
    "a2dp_source.pair",
    ADDRESS_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_PIN, default=""): cv.templatable(cv.string_strict),
            cv.Optional(CONF_REQUIRE_DISCOVERED, default=True): cv.templatable(
                cv.boolean
            ),
        }
    ),
    automation.ApplyCall(
        "pair({}.c_str(), {}.c_str(), {})",
        (
            (CONF_ADDRESS, cg.std_string),
            (CONF_PIN, cg.std_string),
            (CONF_REQUIRE_DISCOVERED, cg.bool_),
        ),
    ),
)
automation.register_apply_action(
    "a2dp_source.pair_incoming",
    ADDRESS_ACTION_SCHEMA,
    automation.ApplyCall("pair_incoming({}.c_str())", ((CONF_ADDRESS, cg.std_string),)),
)
automation.register_apply_action(
    "a2dp_source.confirm_pairing",
    SPEAKER_ACTION_SCHEMA.extend(
        {cv.Required(CONF_APPROVE): cv.templatable(cv.boolean)}
    ),
    automation.ApplyCall("confirm_pairing({})", ((CONF_APPROVE, cg.bool_),)),
)
automation.register_apply_action(
    "a2dp_source.set_bond_page",
    SPEAKER_ACTION_SCHEMA.extend(
        {cv.Required(CONF_PAGE): cv.templatable(cv.int_range(min=0, max=65535))}
    ),
    automation.ApplyCall("set_bond_page({})", ((CONF_PAGE, cg.uint16),)),
)
automation.register_apply_action(
    "a2dp_source.set_discovery_page",
    SPEAKER_ACTION_SCHEMA.extend(
        {cv.Required(CONF_PAGE): cv.templatable(cv.int_range(min=0, max=65535))}
    ),
    automation.ApplyCall("set_discovery_page({})", ((CONF_PAGE, cg.uint16),)),
)
