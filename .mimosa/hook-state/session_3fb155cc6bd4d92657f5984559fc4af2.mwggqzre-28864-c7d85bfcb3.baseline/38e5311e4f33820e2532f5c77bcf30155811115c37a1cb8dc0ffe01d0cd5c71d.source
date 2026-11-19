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
@pytest.mark.parametrize("platform", [PLATFORM_ESP32, PLATFORM_ESP8266])
async def test_publish_json_constants_codegen(platform: str) -> None:
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform

    await automation.build_action(
        _publish_json(**{CONF_QOS: 1, CONF_RETAIN: True}),
        cg.TemplateArguments(),
        [],
    )

    assert (
        "new(act) mqtt::MQTTPublishJsonAction<>(client);\n"
        'act->set_topic(ESPHOME_F("the/topic"));\n'
        "act->set_payload([](ArduinoJson::JsonObject root) -> void {\n"
        '    root["key"] = 1;\n'
        "});\n"
        "act->set_qos([]() -> uint8_t {\n"
        "    return 1;\n"
        "});\n"
        "act->set_retain([]() -> bool {\n"
        "    return true;\n"
        "});\n"
    ) in CORE.cpp_main_section


@pytest.mark.asyncio
@pytest.mark.usefixtures("client")
async def test_publish_json_lambdas_codegen() -> None:
    """Topic, qos and retain are lambdas over the trigger args; the payload also gets ``root``."""
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

    assert (
        "new(act) mqtt::MQTTPublishJsonAction<int>(client);\n"
        "act->set_topic([](int x) -> std::string {\n"
        "    return to_string(x);\n"
        "});\n"
        "act->set_payload([](int x, ArduinoJson::JsonObject root) -> void {\n"
        '    root["x"] = x;\n'
        "});\n"
        "act->set_qos([](int x) -> uint8_t {\n"
        "    return x > 0;\n"
        "});\n"
        "act->set_retain([](int x) -> bool {\n"
        "    return x > 1;\n"
        "});\n"
    ) in CORE.cpp_main_section


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
    """A user lambda gets the trigger args by value, so it may modify them."""
    await automation.build_action(
        _publish_json(**{key: Lambda(body)}), INT_ARG, INT_ARGS
    )

    assert f"{lambda_}\n    {body}\n}}" in CORE.cpp_main_section
