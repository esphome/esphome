from esphome import automation
import esphome.codegen as cg
from esphome.components.wifi import (
    request_wifi_connect_state_listener,
    request_wifi_ip_state_listener,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework
from esphome.core import CORE

from .const import (
    CONF_AUTO_SETUP,
    CONF_FLOW_TYPE,
    CONF_ON_TWT_START,
    CONF_ON_TWT_STOP,
    CONF_ON_TWT_WAKEUP,
    CONF_SETUP_CMD,
    CONF_WAKE_DURATION,
    CONF_WAKE_INTERVAL,
)

DEPENDENCIES = ["wifi"]
CODEOWNERS = ["@rwrozelle"]

# Ordinals match wifi_twt_setup_cmds_t in esp_wifi_he_types.h
SETUP_CMDS = {
    "request": 0,  # TWT_REQUEST
    "suggest": 1,  # TWT_SUGGEST
    "demand": 2,  # TWT_DEMAND
}

wifi_twt_ns = cg.esphome_ns.namespace("wifi_twt")
WiFiTWT = wifi_twt_ns.class_("WiFiTWT", cg.Component)
WiFiTWTStartAction = wifi_twt_ns.class_("WiFiTWTStartAction", automation.Action)
WiFiTWTStopAction = wifi_twt_ns.class_("WiFiTWTStopAction", automation.Action)


def _validate(config):
    interval_ms = config[CONF_WAKE_INTERVAL].total_milliseconds
    duration_ms = config[CONF_WAKE_DURATION].total_milliseconds
    if duration_ms >= interval_ms:
        raise cv.Invalid(
            f"{CONF_WAKE_DURATION} ({duration_ms} ms) must be less than"
            f" {CONF_WAKE_INTERVAL} ({interval_ms} ms)"
        )

    # 802.11ax on-air TWT wake duration is always in 256µs units, max dura=255 → 65280µs ≈ 65ms.
    # The wake_duration_unit=1 field in the ESP-IDF config struct is silently ignored by the
    # firmware when building the on-air frame, so requesting > 65ms silently delivers less.
    max_duration_ms = (255 * 256) // 1000  # = 65 ms
    if duration_ms > max_duration_ms:
        raise cv.Invalid(
            f"{CONF_WAKE_DURATION} ({duration_ms} ms) exceeds the 802.11ax maximum of "
            f"{max_duration_ms} ms (255 × 256 µs). Reduce {CONF_WAKE_DURATION}."
        )

    # ESP-IDF requires SP (interval) >= WD (wake duration) + 10ms guard band.
    # Wire format: mant×2^expn µs (IEEE 802.11ax); SP in µs = interval_ms * 1000.
    sp_us = interval_ms * 1000
    wd_us = duration_ms * 1000
    if sp_us < wd_us + 10000:
        max_wd_ms = max(0, sp_us - 10000) // 1000
        raise cv.Invalid(
            f"ESP-IDF rejects these parameters: wake_interval={sp_us} µs "
            f"must exceed wake_duration={wd_us} µs by at least 10 ms. "
            f"Reduce {CONF_WAKE_DURATION} to ≤ {max_wd_ms} ms or increase {CONF_WAKE_INTERVAL}."
        )
    return config


def _validate_platform(config):
    from esphome.components.esp32 import only_on_variant
    from esphome.components.esp32.const import (
        VARIANT_ESP32C5,
        VARIANT_ESP32C6,
        VARIANT_ESP32C61,
    )

    # only_on_variant reads CORE.target_variant, so position before schema is safe
    only_on_variant(
        supported=[VARIANT_ESP32C5, VARIANT_ESP32C6, VARIANT_ESP32C61],
        msg_prefix="wifi_twt (requires WiFi 6 HE20)",
    )(config)
    cv.only_with_framework(Framework.ESP_IDF)(config)
    return config


def _validate_native_api_conflict(config):
    if (
        CORE.config is not None
        and "native_api" in CORE.config
        and config[CONF_WAKE_INTERVAL].total_milliseconds > 60000
    ):
        raise cv.Invalid(
            "native_api is configured alongside wifi_twt with wake_interval > 60 s. "
            "The native API sends a keepalive ping every 60 s and disconnects at 150 s; "
            "the device must wake within each 60 s window to respond. "
            "Use MQTT or coap_server instead, or reduce wake_interval to ≤ 60 s."
        )
    return config


CONFIG_SCHEMA = cv.All(
    _validate_platform,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WiFiTWT),
            cv.Required(CONF_WAKE_INTERVAL): cv.positive_time_period_milliseconds,
            cv.Required(CONF_WAKE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SETUP_CMD, default="demand"): cv.one_of(
                *SETUP_CMDS, lower=True
            ),
            cv.Optional(CONF_FLOW_TYPE, default="announced"): cv.one_of(
                "announced", "unannounced", lower=True
            ),
            cv.Optional(CONF_AUTO_SETUP, default=True): cv.boolean,
            cv.Optional(CONF_ON_TWT_START): automation.validate_automation({}),
            cv.Optional(CONF_ON_TWT_STOP): automation.validate_automation({}),
            cv.Optional(CONF_ON_TWT_WAKEUP): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
    _validate_native_api_conflict,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_WIFI_TWT")
    request_wifi_ip_state_listener()
    request_wifi_connect_state_listener()

    cg.add(var.set_wake_interval_ms(config[CONF_WAKE_INTERVAL].total_milliseconds))
    cg.add(var.set_wake_duration_ms(config[CONF_WAKE_DURATION].total_milliseconds))
    cg.add(var.set_setup_cmd(SETUP_CMDS[config[CONF_SETUP_CMD]]))
    cg.add(var.set_flow_type(0 if config[CONF_FLOW_TYPE] == "announced" else 1))
    cg.add(var.set_auto_setup(config[CONF_AUTO_SETUP]))

    for conf in config.get(CONF_ON_TWT_START, []):
        await automation.build_callback_automation(
            var, "add_on_start_callback", [], conf
        )
    for conf in config.get(CONF_ON_TWT_STOP, []):
        await automation.build_callback_automation(
            var, "add_on_stop_callback", [], conf
        )
    for conf in config.get(CONF_ON_TWT_WAKEUP, []):
        await automation.build_callback_automation(
            var, "add_on_wakeup_callback", [], conf
        )


def _validate_start_action(config):
    # Re-run interval/duration validation only when both are static (not templates)
    if (
        CONF_WAKE_INTERVAL in config
        and CONF_WAKE_DURATION in config
        and not cg.is_template(config[CONF_WAKE_INTERVAL])
        and not cg.is_template(config[CONF_WAKE_DURATION])
    ):
        _validate(config)
    return config


_FLOW_TYPE_VALUES = {"announced": 0, "unannounced": 1}

START_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(WiFiTWT),
            cv.Optional(CONF_WAKE_INTERVAL): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Optional(CONF_WAKE_DURATION): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
            cv.Optional(CONF_SETUP_CMD): cv.templatable(
                cv.one_of(*SETUP_CMDS, lower=True)
            ),
            cv.Optional(CONF_FLOW_TYPE): cv.templatable(
                cv.one_of(*_FLOW_TYPE_VALUES, lower=True)
            ),
        }
    ),
    _validate_start_action,
)


@automation.register_action(
    "wifi_twt.start",
    WiFiTWTStartAction,
    START_ACTION_SCHEMA,
    synchronous=True,
)
async def wifi_twt_start_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if CONF_WAKE_INTERVAL in config:
        templ = await cg.templatable(
            config[CONF_WAKE_INTERVAL],
            args,
            cg.uint32,
            to_exp=lambda v: v.total_milliseconds,
        )
        cg.add(var.set_wake_interval_ms(templ))
    if CONF_WAKE_DURATION in config:
        templ = await cg.templatable(
            config[CONF_WAKE_DURATION],
            args,
            cg.uint32,
            to_exp=lambda v: v.total_milliseconds,
        )
        cg.add(var.set_wake_duration_ms(templ))
    if CONF_SETUP_CMD in config:
        templ = await cg.templatable(
            config[CONF_SETUP_CMD], args, cg.uint8, to_exp=SETUP_CMDS
        )
        cg.add(var.set_setup_cmd(templ))
    if CONF_FLOW_TYPE in config:
        templ = await cg.templatable(
            config[CONF_FLOW_TYPE], args, cg.uint8, to_exp=_FLOW_TYPE_VALUES
        )
        cg.add(var.set_flow_type(templ))
    return var


@automation.register_action(
    "wifi_twt.stop",
    WiFiTWTStopAction,
    cv.Schema({cv.GenerateID(): cv.use_id(WiFiTWT)}),
    synchronous=True,
)
async def wifi_twt_stop_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
