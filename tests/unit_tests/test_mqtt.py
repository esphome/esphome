from types import SimpleNamespace

from esphome import yaml_util
from esphome.components import mqtt
from esphome.const import (
    CONF_MQTT,
    CONF_TOPIC_NAME_SOURCE,
    TOPIC_NAME_SOURCE_ID,
    TOPIC_NAME_SOURCE_NAME,
)


def _core_with_topic_source(topic_name_source, friendly_name="Friendly Name"):
    mqtt_config = {}
    if topic_name_source is not None:
        mqtt_config[CONF_TOPIC_NAME_SOURCE] = topic_name_source
    return SimpleNamespace(config={CONF_MQTT: mqtt_config}, friendly_name=friendly_name)


def test_get_default_topic_for_uses_name_by_default(monkeypatch):
    data = SimpleNamespace(topic_prefix="test")
    core = _core_with_topic_source(TOPIC_NAME_SOURCE_NAME)
    monkeypatch.setattr(mqtt, "CORE", core)

    topic = mqtt.get_default_topic_for(data, "sensor", "Мой Датчик", "state")

    assert topic == "test/sensor/мой_датчик/state"


def test_get_default_topic_for_uses_object_id_when_configured(monkeypatch):
    data = SimpleNamespace(topic_prefix="test", object_id="sensor_id")
    core = _core_with_topic_source(TOPIC_NAME_SOURCE_ID)
    monkeypatch.setattr(mqtt, "CORE", core)

    topic = mqtt.get_default_topic_for(data, "sensor", "Мой Датчик", "state")

    assert topic == "test/sensor/sensor_id/state"


def test_get_default_topic_for_uses_unicode_object_id(monkeypatch):
    data = SimpleNamespace(topic_prefix="test", object_id="модуль_1")
    core = _core_with_topic_source(TOPIC_NAME_SOURCE_ID)
    monkeypatch.setattr(mqtt, "CORE", core)

    topic = mqtt.get_default_topic_for(data, "sensor", "Мой Датчик", "state")

    assert topic == "test/sensor/модуль_1/state"


def test_get_default_topic_for_falls_back_to_name_when_no_object_id(monkeypatch):
    data = SimpleNamespace(topic_prefix="test")
    core = _core_with_topic_source(TOPIC_NAME_SOURCE_ID)
    monkeypatch.setattr(mqtt, "CORE", core)

    topic = mqtt.get_default_topic_for(data, "sensor", "Мой Датчик", "state")

    assert topic == "test/sensor/мой_датчик/state"


def test_get_default_topic_for_uses_friendly_name_when_name_empty(monkeypatch):
    data = SimpleNamespace(topic_prefix="test")
    core = _core_with_topic_source(
        TOPIC_NAME_SOURCE_NAME, friendly_name="Гостиная Датчик"
    )
    monkeypatch.setattr(mqtt, "CORE", core)

    topic = mqtt.get_default_topic_for(data, "sensor", "", "state")

    assert topic == "test/sensor/гостиная_датчик/state"


def test_topic_name_source_yaml_fixture_provides_config_example(fixture_path):
    yaml_path = fixture_path / "mqtt" / "topic_name_source.yaml"

    config = yaml_util.load_yaml(yaml_path)

    assert config["mqtt"]["topic_name_source"] == "id"
    assert config["sensor"][0]["name"] == "Температура Гостиной"
