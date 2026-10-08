"""Tests for the api wizard: schema, final validation and generated tables."""

from collections.abc import Callable
import json
from pathlib import Path
import random
import re
import string
import textwrap

import pytest

from esphome.components.api import wizard
from esphome.components.homeassistant.switch import SUPPORTED_DOMAINS as SWITCH_DOMAINS
from esphome.config import load_config
from esphome.core import CORE
from esphome.helpers import fnv1_hash, fnv1a_32bit_hash, zstd_module
from tests.component_tests.helpers import get_define_value

ESP32_HEADER = """
    esphome:
      name: test

    esp32:
      variant: esp32

    wifi:
      ssid: test
      password: testtest

    logger:
    """

ESP8266_HEADER = """
    esphome:
      name: test

    esp8266:
      board: d1_mini

    wifi:
      ssid: test
      password: testtest

    logger:
    """

ENTITIES = """
    switch:
      - platform: template
        id: sw
        name: Switch
        optimistic: true
      - platform: template
        id: internal_sw
        name: Internal switch
        internal: true
        optimistic: true
      - platform: template
        id: unnamed_sw
        optimistic: true
      - platform: homeassistant
        id: ha_switch
        %(light)s
    sensor:
      - platform: template
        id: template_sensor
        name: Template sensor
      - platform: homeassistant
        id: ha_plain_sensor
        entity_id: sensor.plain
      - platform: homeassistant
        id: ha_sensor
        %(sensor)s
    binary_sensor:
      - platform: homeassistant
        id: ha_binary
        %(binary)s
    text_sensor:
      - platform: homeassistant
        id: ha_text
        %(text)s
    number:
      - platform: homeassistant
        id: ha_number
        %(number)s
    text:
      - platform: homeassistant
        id: ha_txt
        %(text_entity)s
    select:
      - platform: homeassistant
        id: ha_select
        %(select)s
    button:
      - platform: homeassistant
        id: ha_button
        %(button)s
    """


def build_entities(inputs: set[str]) -> str:
    """The test entities. The homeassistant ones that are wizard inputs set no entity_id, the others do."""
    ids = {
        "light": ("ha_switch", "entity_id: light.lamp"),
        "sensor": ("ha_sensor", "entity_id: sensor.a"),
        "binary": ("ha_binary", "entity_id: binary_sensor.a"),
        "text": ("ha_text", "entity_id: sensor.b"),
        "number": ("ha_number", "entity_id: number.a"),
        "text_entity": ("ha_txt", "entity_id: text.a"),
        "select": ("ha_select", "entity_id: select.a"),
        "button": ("ha_button", "entity_id: button.a"),
    }
    return ENTITIES % {
        key: "" if name in inputs else yaml for key, (name, yaml) in ids.items()
    }


GOOD_PAGES = """
    api:
      wizard:
        pages:
          - title: Setup
            description: "[%key:component::domain::section::name%]"
            entities:
              - id: sw
                description: Enable
              - id: sw
          - inputs:
              - entity: ha_sensor
                description: Weather
                target:
                  entity:
                    - integration: met
                      domain: sensor
                      device_class: [temperature, humidity]
                      supported_features: weather.WeatherEntityFeature.FORECAST_DAILY
                    - domain: [sensor, number]
              - entity: ha_binary
              - entity: ha_text
              - entity: ha_number
              - entity: ha_switch
              - entity: ha_txt
              - entity: ha_select
              - entity: ha_button
              - id: weather_input
                target:
                  entity:
                    - domain: weather
              - id: unset_input
    """


def write_config(
    tmp_path: Path, header: str, api: str, yaml_entities: str | None = None
) -> Path:
    """Write a config. By default the homeassistant entities that the wizard does not use as inputs set an entity_id."""
    path = tmp_path / "test.yaml"
    if yaml_entities is None:
        yaml_entities = build_entities(set(re.findall(r"entity: (\w+)", api)))
    path.write_text(
        textwrap.dedent(header) + textwrap.dedent(yaml_entities) + textwrap.dedent(api)
    )
    return path


def write_input_config(tmp_path: Path, header: str) -> Path:
    """Write a config with GOOD_PAGES."""
    return write_config(tmp_path, header, GOOD_PAGES)


def config_errors(path: Path) -> list[str]:
    CORE.config_path = path
    return [str(err.msg) for err in load_config({}).errors]


def wizard_api(pages: str) -> str:
    return "api:\n  wizard:\n    pages:\n" + textwrap.indent(
        textwrap.dedent(pages).strip("\n"), "      "
    )


def test_no_wizard_emits_nothing(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(
        write_config(
            tmp_path,
            ESP32_HEADER,
            "api:\n",
            "sensor:\n  - platform: homeassistant\n    id: s\n    entity_id: sensor.s\n",
        )
    )

    assert "API_WIZARD_PAGES" not in main_cpp
    assert 's->set_entity_id("sensor.s");' in main_cpp
    assert not any(define.name == "USE_API_WIZARD" for define in CORE.defines)


def test_valid_wizard_has_no_errors(tmp_path: Path) -> None:
    assert config_errors(write_input_config(tmp_path, ESP32_HEADER)) == []


def wizard_inputs(inputs: str) -> str:
    return wizard_api("- inputs:\n" + textwrap.indent(textwrap.dedent(inputs), "    "))


def test_switch_input_with_supported_domains_is_valid(tmp_path: Path) -> None:
    api = wizard_inputs(
        """
        - entity: ha_switch
          target:
            entity:
              - domain: [light, fan]
        """
    )

    assert config_errors(write_config(tmp_path, ESP32_HEADER, api)) == []


@pytest.mark.parametrize(
    ("api", "message"),
    [
        pytest.param(
            wizard_inputs("- entity: template_sensor"),
            "must be a homeassistant",
            id="input-not-homeassistant",
        ),
        pytest.param(
            wizard_inputs("- entity: sw"),
            "must be a homeassistant",
            id="input-not-homeassistant-switch",
        ),
        pytest.param(
            wizard_inputs(
                """
                - entity: ha_switch
                  target:
                    entity:
                      - domain: [light, valve]
                """
            ),
            "does not support the domain(s) valve",
            id="switch-unsupported-domain",
        ),
        pytest.param(
            wizard_inputs(
                """
                - entity: ha_switch
                  target:
                    entity:
                      - integration: hue
                """
            ),
            "must set domain",
            id="switch-filter-without-domain",
        ),
        pytest.param(
            wizard_inputs("- entity: ha_sensor\n- entity: ha_sensor"),
            "'ha_sensor' is used more than once",
            id="duplicate-input",
        ),
    ],
)
def test_invalid_inputs_are_rejected(tmp_path: Path, api: str, message: str) -> None:
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, api))

    assert any(message in error for error in errors), errors


def test_hash_collision_is_rejected(tmp_path: Path) -> None:
    entities = "".join(
        f"sensor:\n  - platform: homeassistant\n    id: {name}\n"
        if index == 0
        else f"  - platform: homeassistant\n    id: {name}\n"
        for index, name in enumerate(("a179599", "a362382"))
    )
    api = wizard_inputs("- entity: a179599\n- entity: a362382")
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, api, entities))

    assert any("have the same hash" in error for error in errors), errors


@pytest.mark.parametrize(
    "extra",
    [
        "",
        "    entity_id: sensor.default\n",
    ],
)
def test_entity_id_is_optional_only_for_wizard_inputs(
    tmp_path: Path, extra: str
) -> None:
    entities = f"sensor:\n  - platform: homeassistant\n    id: lonely\n{extra}"
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, "api:\n", entities))

    if extra:
        assert errors == []
    else:
        assert any(
            "entity_id is required unless this entity is a wizard input" in error
            for error in errors
        ), errors


def test_entity_id_is_optional_for_an_input_and_its_domain_is_checked(
    tmp_path: Path,
) -> None:
    entities = "switch:\n  - platform: homeassistant\n    id: ha_switch\n    entity_id: climate.heater\n"
    errors = config_errors(
        write_config(
            tmp_path, ESP32_HEADER, wizard_inputs("- entity: ha_switch"), entities
        )
    )

    # The YAML entity ID is still checked against the platform's domains
    assert any("not supported by the switch platform" in error for error in errors)


@pytest.mark.parametrize(
    ("pages", "message"),
    [
        pytest.param(
            "- title: Empty",
            "at least one of entities, inputs",
            id="page-without-fields",
        ),
        pytest.param(
            """
            - entities: []
            """,
            "length of value must be at least 1",
            id="empty-entities-list",
        ),
        pytest.param(
            """
            - entities: [{id: internal_sw}]
            """,
            "Entity 'internal_sw' is internal",
            id="internal-entity",
        ),
        pytest.param(
            """
            - entities: [{id: unnamed_sw}]
            """,
            "Entity 'unnamed_sw' is internal",
            id="unnamed-entity-is-internal",
        ),
        pytest.param(
            """
            - inputs:
                - entity: ha_sensor
                  target:
                    entity:
                      - {}
            """,
            "at least one of",
            id="empty-filter",
        ),
        pytest.param(
            """
            - inputs:
                - entity: ha_sensor
                  target:
                    entity: []
            """,
            "length of value must be at least 1",
            id="no-filters",
        ),
        pytest.param(
            """
            - inputs:
                - entity: ha_sensor
                  target:
                    entity:
                      - domain: []
            """,
            "length of value must be at least 1",
            id="empty-domain-list",
        ),
        pytest.param(
            """
            - inputs:
                - entity: ha_sensor
                  target:
                    entity:
                      - supported_features: FEATURE
            """.replace("FEATURE", "x" * 128),
            "length of value must be at most 127",
            id="long-feature",
        ),
        pytest.param(
            """
            - description: TEXT
              entities: [{id: sw}]
            """.replace("TEXT", "x" * 256),
            "length of value must be at most 255",
            id="long-description",
        ),
    ],
)
def test_invalid_wizards_are_rejected(tmp_path: Path, pages: str, message: str) -> None:
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, wizard_api(pages)))

    assert any(message in error for error in errors), errors


def test_no_pages_is_rejected(tmp_path: Path) -> None:
    errors = config_errors(
        write_config(tmp_path, ESP32_HEADER, "api:\n  wizard:\n    pages: []\n")
    )

    assert any("length of value must be at least 1" in error for error in errors), (
        errors
    )


def test_input_is_set_condition(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    interval = """
    interval:
      - interval: 1h
        then:
          - if:
              condition:
                api.wizard.input_is_set: weather_input
              then:
                - logger.log: set
          - if:
              condition:
                api.wizard.input_is_set:
                  id: unset_input
              then:
                - logger.log: set
    """
    path = write_input_config(tmp_path, ESP32_HEADER)
    path.write_text(path.read_text() + textwrap.dedent(interval))
    main_cpp = generate_main(path)

    assert "weather_input->has_entity_id()" in main_cpp
    assert "unset_input->has_entity_id()" in main_cpp


@pytest.mark.parametrize(
    ("inputs", "message"),
    [
        pytest.param(
            "- description: Neither",
            "exactly one of id, entity",
            id="neither-id-nor-entity",
        ),
        pytest.param(
            "- id: standalone\n  entity: ha_sensor",
            "more than one of id, entity",
            id="both-id-and-entity",
        ),
        pytest.param(
            "- entity: ha_sensor\n  entity_id: sensor.a",
            "extra keys not allowed",
            id="entity-id-on-linked-input",
        ),
        pytest.param(
            "- id: standalone\n  entity_id: weather.home",
            "extra keys not allowed",
            id="entity-id-on-standalone-input",
        ),
        pytest.param(
            "- entity: ha_sensor\n- entity: ha_sensor",
            "used more than once",
            id="duplicate-linked-input",
        ),
    ],
)
def test_standalone_and_linked_input_rules(
    tmp_path: Path, inputs: str, message: str
) -> None:
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, wizard_inputs(inputs)))

    assert any(message in error for error in errors), errors


def test_standalone_input_is_valid(tmp_path: Path) -> None:
    api = wizard_inputs("- id: standalone")

    assert config_errors(write_config(tmp_path, ESP32_HEADER, api)) == []


WIZARD_DEFINES = {
    "USE_API_WIZARD",
    "USE_API_WIZARD_INPUTS",
    "USE_API_WIZARD_LINKED_INPUTS",
    "USE_API_WIZARD_STANDALONE_INPUTS",
}


@pytest.mark.parametrize(
    ("pages", "expected"),
    [
        pytest.param(
            "- entities: [{id: sw}]",
            {"USE_API_WIZARD"},
            id="entity-only",
        ),
        pytest.param(
            "- inputs: [{id: standalone}]",
            {
                "USE_API_WIZARD",
                "USE_API_WIZARD_INPUTS",
                "USE_API_WIZARD_STANDALONE_INPUTS",
            },
            id="standalone-input-without-filter",
        ),
        pytest.param(
            "- inputs: [{id: standalone, target: {entity: [{domain: weather}]}}]",
            {
                "USE_API_WIZARD",
                "USE_API_WIZARD_INPUTS",
                "USE_API_WIZARD_STANDALONE_INPUTS",
            },
            id="standalone-input-with-filter",
        ),
        pytest.param(
            "- inputs: [{entity: ha_sensor}]",
            {
                "USE_API_WIZARD",
                "USE_API_WIZARD_INPUTS",
                "USE_API_WIZARD_LINKED_INPUTS",
            },
            id="linked-sensor-without-filter",
        ),
        pytest.param(
            "- inputs: [{entity: ha_sensor, target: {entity: [{domain: sensor}]}}]",
            {
                "USE_API_WIZARD",
                "USE_API_WIZARD_INPUTS",
                "USE_API_WIZARD_LINKED_INPUTS",
            },
            id="linked-sensor-with-filter",
        ),
        pytest.param(
            # A switch without a target gets the default filter
            "- inputs: [{entity: ha_switch}]",
            {
                "USE_API_WIZARD",
                "USE_API_WIZARD_INPUTS",
                "USE_API_WIZARD_LINKED_INPUTS",
            },
            id="linked-switch-default-filter",
        ),
        pytest.param(
            "- entities: [{id: sw}]\n  inputs: [{entity: ha_sensor, target: {entity: [{domain: sensor}]}}, {id: standalone}]",
            WIZARD_DEFINES,
            id="everything",
        ),
    ],
)
def test_only_the_defines_the_wizard_needs_are_emitted(
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
    pages: str,
    expected: set[str],
) -> None:
    api = wizard_api(pages)
    generate_main(write_config(tmp_path, ESP32_HEADER, api))

    assert {d.name for d in CORE.defines} & WIZARD_DEFINES == expected


@pytest.mark.parametrize(
    ("entity", "bad_domain"),
    [
        ("ha_txt", "select"),
        ("ha_select", "text"),
        ("ha_button", "switch"),
        ("ha_number", "sensor"),
    ],
)
def test_text_select_and_button_inputs_keep_to_their_domains(
    tmp_path: Path, entity: str, bad_domain: str
) -> None:
    api = wizard_inputs(
        f"- entity: {entity}\n  target:\n    entity:\n      - domain: {bad_domain}"
    )
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, api))

    assert any(
        f"does not support the domain(s) {bad_domain}" in error for error in errors
    ), errors


@pytest.mark.parametrize("entity", ["ha_txt", "ha_select", "ha_button", "ha_number"])
def test_text_select_and_button_filters_must_set_a_domain(
    tmp_path: Path, entity: str
) -> None:
    api = wizard_inputs(
        f"- entity: {entity}\n  target:\n    entity:\n      - integration: hue"
    )
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, api))

    assert any("must set domain" in error for error in errors), errors


def test_number_input_accepts_input_number(tmp_path: Path) -> None:
    api = wizard_inputs(
        "- entity: ha_number\n  target:\n    entity:\n      - domain: input_number"
    )

    assert config_errors(write_config(tmp_path, ESP32_HEADER, api)) == []


def test_text_select_and_button_default_to_their_domains(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))
    document = json.loads(zstd_module().decompress(blob_in(main_cpp)))
    filters = {
        entry["key"]: entry.get("entity_filters")
        for page in document["pages"]
        for entry in page.get("inputs", [])
    }

    # ha_txt and ha_button have no target, so they take every supported domain
    assert filters[fnv1_hash("ha_txt")] == [{"domain": ["input_text", "text"]}]
    assert filters[fnv1_hash("ha_button")] == [{"domain": ["button", "input_button"]}]
    assert filters[fnv1_hash("ha_select")] == [{"domain": ["input_select", "select"]}]
    # A number takes number and input_number entities, and the other sensors take anything
    assert filters[fnv1_hash("ha_number")] == [{"domain": ["input_number", "number"]}]
    assert filters[fnv1_hash("ha_binary")] is None
    for entity in ("ha_txt", "ha_select", "ha_button"):
        assert f"{entity}->set_entity_id(api_wizard_input_{entity});" in main_cpp


@pytest.mark.parametrize(
    "entity_yaml",
    [
        "text:\n  - platform: homeassistant\n    id: lonely\n",
        "select:\n  - platform: homeassistant\n    id: lonely\n",
        "button:\n  - platform: homeassistant\n    id: lonely\n",
    ],
)
def test_new_platforms_need_an_entity_id_unless_they_are_inputs(
    tmp_path: Path, entity_yaml: str
) -> None:
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, "api:\n", entity_yaml))

    assert any(
        "entity_id is required unless this entity is a wizard input" in error
        for error in errors
    ), errors


DEVICES_HEADER = """
    esphome:
      name: test
      devices:
        - id: kitchen_dev
          name: Kitchen

    esp32:
      variant: esp32

    wifi:
      ssid: test
      password: testtest

    logger:
    """

JSON_ENTITIES = """
    switch:
      - platform: template
        id: sw
        name: Switch
        optimistic: true
      - platform: template
        id: sw2
        name: Kitchen Switch
        device_id: kitchen_dev
        optimistic: true
      - platform: homeassistant
        id: ha_switch
    sensor:
      - platform: homeassistant
        id: ha_sensor
    """

JSON_PAGES = """
    - title: Audio
      description: Pick
      entities:
        - id: sw
          description: Enable
        - id: sw2
      inputs:
        - id: weather
          description: Weather
          target:
            entity:
              - integration: met
                domain: weather
                device_class: [temperature, humidity]
              - domain: [weather, sensor]
        - entity: ha_switch
    - inputs: [{entity: ha_sensor}]
    """


def blob_in(main_cpp: str) -> bytes:
    """The compressed wizard that the generated code puts in flash."""
    match = re.search(
        r"const uint8_t esphome::api::API_WIZARD_DATA\[\] PROGMEM = \{([^}]*)\};",
        main_cpp,
    )
    assert match is not None
    return bytes(int(byte) for byte in match.group(1).split(", "))


def entity_hash(main_cpp: str, variable: str) -> int:
    """The key the generated code passes to configure_entity_, which ListEntities then sends."""
    match = re.search(
        rf'App\.register_switch\({variable}, "[^"]*", (\d+)UL, \d+\)', main_cpp
    )
    assert match is not None, variable
    return int(match.group(1))


def test_the_blob_is_the_exact_json_document(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(
        write_config(tmp_path, DEVICES_HEADER, wizard_api(JSON_PAGES), JSON_ENTITIES)
    )
    blob = blob_in(main_cpp)

    text = zstd_module().decompress(blob).decode("utf-8")
    expected = {
        "version": 1,
        "pages": [
            {
                "title": "Audio",
                "description": "Pick",
                "entities": [
                    # No device_id for the main device, and no description when unset
                    {"key": entity_hash(main_cpp, "sw"), "description": "Enable"},
                    {
                        "key": entity_hash(main_cpp, "sw2"),
                        "device_id": fnv1a_32bit_hash("kitchen_dev"),
                    },
                ],
                "inputs": [
                    {
                        "key": fnv1_hash("weather"),
                        "description": "Weather",
                        "entity_filters": [
                            {
                                "integration": "met",
                                "domain": ["weather"],
                                "device_class": ["temperature", "humidity"],
                            },
                            {"domain": ["weather", "sensor"]},
                        ],
                    },
                    {
                        "key": fnv1_hash("ha_switch"),
                        # The default filter of a switch
                        "entity_filters": [{"domain": SWITCH_DOMAINS}],
                    },
                ],
            },
            {"inputs": [{"key": fnv1_hash("ha_sensor")}]},
        ],
    }
    assert text == json.dumps(
        expected, separators=(",", ":"), sort_keys=True, ensure_ascii=False
    )
    # The device id is the one the generated code gives the device
    assert f"set_device_id({fnv1a_32bit_hash('kitchen_dev')})" in main_cpp
    assert get_define_value("API_WIZARD_DATA_SIZE") == str(len(blob))


def test_the_blob_is_deterministic_and_the_same_on_every_platform(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    api = wizard_api(JSON_PAGES)
    first = blob_in(
        generate_main(write_config(tmp_path, DEVICES_HEADER, api, JSON_ENTITIES))
    )
    # Compressing the same document again gives the same bytes
    document = wizard.wizard_document(CORE.config["api"]["wizard"], CORE.config)
    again = zstd_module().compress(
        json.dumps(
            document, separators=(",", ":"), sort_keys=True, ensure_ascii=False
        ).encode("utf-8"),
        level=wizard.WIZARD_ZSTD_LEVEL,
    )

    assert again == first
    assert first[:4] == b"\x28\xb5\x2f\xfd"  # a zstd frame


def test_the_blob_is_in_flash_on_esp8266(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP8266_HEADER))

    assert "esphome::api::API_WIZARD_DATA[] PROGMEM = {" in main_cpp
    assert (
        "const api::WizardInputEntry esphome::api::API_WIZARD_INPUTS[] PROGMEM = {"
        in main_cpp
    )
    # Only the compressed data and the input table are emitted, no strings or row tables
    assert "api_wizard_str" not in main_cpp
    assert "WizardEntityRow" not in main_cpp


def test_inputs_have_buffers_hashes_and_a_table(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))

    # The buffer holds the YAML entity ID until the wizard sets another
    assert 'static char api_wizard_input_ha_sensor[256] = "";' in main_cpp
    entities = (
        "ha_sensor",
        "ha_binary",
        "ha_text",
        "ha_number",
        "ha_switch",
        "ha_txt",
        "ha_select",
        "ha_button",
        "weather_input",
        "unset_input",
    )
    for entity in entities:
        assert f"{{{fnv1_hash(entity)}u, api_wizard_input_{entity}}}" in main_cpp
    assert get_define_value("API_WIZARD_INPUT_COUNT") == str(len(entities))
    for entity in entities[:-2]:
        assert f"{entity}->set_entity_id(api_wizard_input_{entity});" in main_cpp
    # The standalone inputs are objects that read the same buffer
    assert "WizardInput(api_wizard_input_weather_input)" in main_cpp
    assert "WizardInput(api_wizard_input_unset_input)" in main_cpp
    # Not an input, so it keeps its constant
    assert 'ha_plain_sensor->set_entity_id("sensor.plain");' in main_cpp


def test_an_entity_without_a_name_cannot_be_in_the_wizard(tmp_path: Path) -> None:
    yaml_entities = 'switch:\n  - platform: template\n    id: nameless\n    name: ""\n    optimistic: true\n'
    errors = config_errors(
        write_config(
            tmp_path,
            ESP32_HEADER,
            wizard_api("- entities: [{id: nameless}]"),
            yaml_entities,
        )
    )

    assert any("has no name of its own" in error for error in errors), errors


def test_a_wizard_too_big_for_one_message_is_rejected(tmp_path: Path) -> None:
    # Random text does not compress, so this needs more than the limit even compressed
    rng = random.Random(1)
    alphabet = string.ascii_letters + string.digits
    entities = ",\n".join(
        "{id: sw, description: "
        + "".join(rng.choice(alphabet) for _ in range(255))
        + "}"
        for _ in range(400)
    )
    api = wizard_api(f"- entities: [\n{entities}\n]")
    errors = config_errors(write_config(tmp_path, ESP32_HEADER, api))

    assert any(
        "The compressed wizard is" in error and "bytes over the 65512 bytes" in error
        for error in errors
    ), errors


def test_repeated_text_compresses_well_inside_the_limit(tmp_path: Path) -> None:
    # 400 identical entities are far over the limit as JSON, but compress to very little
    entities = ",\n".join(["{id: sw, description: " + "x" * 255 + "}"] * 400)
    api = wizard_api(f"- entities: [\n{entities}\n]")

    assert config_errors(write_config(tmp_path, ESP32_HEADER, api)) == []


@pytest.mark.parametrize(
    ("platform", "entity_id"),
    [
        ("sensor", "sensor.a"),
        ("switch", "light.a"),
        ("text", "text.a"),
        ("button", "button.a"),
    ],
)
def test_a_linked_entity_must_not_set_an_entity_id(
    tmp_path: Path, platform: str, entity_id: str
) -> None:
    yaml_entities = (
        f"{platform}:\n  - platform: homeassistant\n    id: fixed\n"
        f"    entity_id: {entity_id}\n"
    )
    errors = config_errors(
        write_config(
            tmp_path, ESP32_HEADER, wizard_inputs("- entity: fixed"), yaml_entities
        )
    )

    assert any(
        "'fixed' has an entity_id set in its configuration" in error
        and "Remove entity_id" in error
        for error in errors
    ), errors


def test_an_entity_with_an_entity_id_that_is_no_input_is_valid(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))

    # Static entries keep their literal and are kept apart from the wizard buffers
    assert 'ha_plain_sensor->set_entity_id("sensor.plain");' in main_cpp
    assert "api_wizard_input_ha_plain_sensor" not in main_cpp
    # Every buffer starts empty
    assert '] = "sensor.' not in main_cpp.split("API_WIZARD_INPUTS")[0]
