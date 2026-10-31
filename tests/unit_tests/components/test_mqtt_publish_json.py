"""Tests for the code generated for mqtt.publish_json."""

import pytest

from esphome import automation
import esphome.codegen as cg
from esphome.components.mqtt import MQTTClientComponent
from esphome.const import (
    CONF_ID,
    CONF_PAYLOAD,
    CONF_QOS,
    CONF_RETAIN,
    CONF_TOPIC,
    CONF_TYPE_ID,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID, Lambda
from esphome.types import ConfigType

INT_ARG = cg.TemplateArguments(cg.int_)
INT_ARGS = [(cg.int_, "x")]


def _publish_json(**body: object) -> ConfigType:
    """Config for an ``mqtt.publish_json`` on ``client``, typed as the registry declares it."""
    return {
        "mqtt.publish_json": {
            CONF_ID: ID("client", type=MQTTClientComponent),
            CONF_TOPIC: "the/topic",
            CONF_PAYLOAD: Lambda('root["key"] = 1;'),
            CONF_QOS: 0,
            CONF_RETAIN: False,
            **body,
        },
        CONF_TYPE_ID: ID(
            "act",
            is_declaration=True,
            type=automation.ACTION_REGISTRY["mqtt.publish_json"].type_id,
            is_manual=False,
        ),
    }


@pytest.fixture
def client() -> None:
    CORE.config = {}
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    cg.new_Pvariable(ID("client", is_declaration=True, type=MQTTClientComponent))


@pytest.mark.asyncio
@pytest.mark.usefixtures("client")
@pytest.mark.parametrize(
    ("platform", "topic"),
    [
        (PLATFORM_ESP32, '"the/topic"'),
        (PLATFORM_ESP8266, 'progmem_string(ESPHOME_F("the/topic"))'),
    ],
)
async def test_publish_json_constants_codegen(platform: str, topic: str) -> None:
    """Constants are passed straight to ``publish_json``; the topic stays in flash on ESP8266."""
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform

    await automation.build_action(
        _publish_json(**{CONF_QOS: 1, CONF_RETAIN: True}),
        cg.TemplateArguments(),
        [],
    )

    assert "new(act) ApplyAction<esphome__act__fn>();" in CORE.cpp_main_section
    assert (
        "static void esphome__act__fn() {\n"
        f"  ::client->publish_json({topic}, [&](ArduinoJson::JsonObject root) {{ "
        "[](ArduinoJson::JsonObject root) -> void {\n"
        '    root["key"] = 1;\n'
        "  }(root); }, 1, true);\n"
        "}\n"
    ) in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("client")
async def test_publish_json_lambdas_codegen() -> None:
    """Topic, qos and retain are computed in that order before the publish; the payload also gets ``root``."""
    await automation.build_action(
        _publish_json(
            **{
                CONF_TOPIC: Lambda("return to_string(x);"),
                CONF_PAYLOAD: Lambda('root["x"] = x;'),
                CONF_QOS: Lambda("return x > 0;"),
                CONF_RETAIN: Lambda("return x > 1;"),
            }
        ),
        INT_ARG,
        INT_ARGS,
    )

    assert "new(act) ApplyAction<esphome__act__fn, int>();" in CORE.cpp_main_section
    assert (
        "static void esphome__act__fn(const std::remove_cvref_t<int> & x) {\n"
        "  {\n"
        "    const std::string esphome__topic = [](int x) -> std::string {\n"
        "    return to_string(x);\n"
        "  }(x);\n"
        "    const uint8_t esphome__qos = [](int x) -> uint8_t {\n"
        "    return x > 0;\n"
        "  }(x);\n"
        "    const bool esphome__retain = [](int x) -> bool {\n"
        "    return x > 1;\n"
        "  }(x);\n"
        "    ::client->publish_json(esphome__topic, [&](ArduinoJson::JsonObject root) { "
        "[](int x, ArduinoJson::JsonObject root) -> void {\n"
        '    root["x"] = x;\n'
        "  }(x, root); }, esphome__qos, esphome__retain);\n"
        "  }\n"
        "}\n"
    ) in CORE.cpp_global_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("client")
async def test_publish_json_merged_actions_keep_their_locals_apart() -> None:
    """Two merged publishes each bind their values in their own block."""
    first = _publish_json(**{CONF_TOPIC: Lambda("return to_string(x);")})
    second = _publish_json(**{CONF_TOPIC: Lambda("return to_string(x + 1);")})
    second[CONF_TYPE_ID] = ID(
        "act2", is_declaration=True, type=automation.ApplyAction, is_manual=False
    )

    result = await automation.build_action_list([first, second], INT_ARG, INT_ARGS)

    assert [str(action) for action in result] == ["act"]
    text = CORE.cpp_global_section
    assert text.count("  {\n    const std::string esphome__topic = ") == 2
    assert text.count("esphome__topic, [&]") == 2


@pytest.mark.asyncio
@pytest.mark.usefixtures("client")
@pytest.mark.parametrize(
    ("key", "body", "lambda_"),
    [
        (CONF_TOPIC, "x *= 2; return to_string(x);", "[](int x) -> std::string {"),
        (CONF_TOPIC, "return to_string(x *= 2);", "[](int x) -> std::string {"),
        (CONF_QOS, "x *= 0; return x;", "[](int x) -> uint8_t {"),
        (CONF_QOS, "return x *= 0;", "[](int x) -> uint8_t {"),
        (CONF_RETAIN, "x *= 2; return x > 0;", "[](int x) -> bool {"),
        (CONF_RETAIN, "return (x *= 2) > 0;", "[](int x) -> bool {"),
        (
            CONF_PAYLOAD,
            'x *= 2; root["x"] = x;',
            "[](int x, ArduinoJson::JsonObject root) -> void {",
        ),
    ],
)
async def test_publish_json_lambdas_take_args_by_value(
    key: str, body: str, lambda_: str
) -> None:
    """A user lambda gets the trigger args by value, so it may modify them.

    It is called rather than reduced, so even a single return modifies only its own copy.
    """
    await automation.build_action(
        _publish_json(**{key: Lambda(body)}), INT_ARG, INT_ARGS
    )

    assert f"{lambda_}\n    {body}\n  }}(x" in CORE.cpp_global_section
