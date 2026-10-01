"""Tests for the api wizard: schema, final validation and generated tables."""

from collections.abc import Callable
from pathlib import Path
import re
import textwrap

import pytest

from esphome.components.homeassistant.switch import SUPPORTED_DOMAINS as SWITCH_DOMAINS
from esphome.config import load_config
from esphome.core import CORE
from esphome.helpers import fnv1_hash
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
      - platform: homeassistant
        id: ha_default_sensor
        entity_id: sensor.default
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
    """


def build_entities(entity_ids: bool) -> str:
    """The test entities. The homeassistant ones that GOOD_PAGES uses as inputs get a YAML entity_id or not."""
    ids = {
        "light": "entity_id: light.lamp",
        "sensor": "entity_id: sensor.a",
        "binary": "entity_id: binary_sensor.a",
        "text": "entity_id: sensor.b",
        "number": "entity_id: number.a",
    }
    return ENTITIES % {k: v if entity_ids else "" for k, v in ids.items()}


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
              - entity: ha_default_sensor
              - entity: ha_binary
              - entity: ha_text
              - entity: ha_number
              - entity: ha_switch
              - id: weather_input
                target:
                  entity:
                    - domain: weather
              - id: unset_input
    """


def write_config(
    tmp_path: Path, header: str, api: str, yaml_entities: str | None = None
) -> Path:
    """Write a config. By default the homeassistant entities have an entity_id."""
    path = tmp_path / "test.yaml"
    if yaml_entities is None:
        yaml_entities = build_entities(True)
    path.write_text(
        textwrap.dedent(header) + textwrap.dedent(yaml_entities) + textwrap.dedent(api)
    )
    return path


def write_input_config(tmp_path: Path, header: str) -> Path:
    """Write a config with GOOD_PAGES, whose inputs have no entity_id."""
    return write_config(tmp_path, header, GOOD_PAGES, build_entities(False))


def config_errors(path: Path) -> list[str]:
    CORE.config_path = path
    return [str(err.msg) for err in load_config({}).errors]


def wizard_api(pages: str) -> str:
    return "api:\n  wizard:\n    pages:\n" + textwrap.indent(
        textwrap.dedent(pages).strip("\n"), "      "
    )


def test_generates_flash_tables(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    """Every row is emitted as a constant table, and the pages table is the exported one."""
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))

    assert (
        "static const esphome::api::WizardEntityRow api_wizard_0[] = {\n"
        '  {[]() -> esphome::EntityBase * { return sw; }, "Enable"},\n'
        '  {[]() -> esphome::EntityBase * { return sw; }, ""},\n};'
    ) in main_cpp
    # A single string is a one entry list, and an unset list is an empty span
    assert 'static const char *const api_wizard_1[] = {\n  "sensor",\n};' in main_cpp
    assert (
        '{"met", {api_wizard_1, 1}, {api_wizard_2, 2}, {api_wizard_3, 1}},' in main_cpp
    )
    assert '{"", {api_wizard_4, 2}, {nullptr, 0}, {nullptr, 0}},' in main_cpp
    assert (
        '{"Setup", "[%key:component::domain::section::name%]", {api_wizard_0, 2}, {nullptr, 0}}'
        in main_cpp
    )
    assert any(define.name == "USE_API_WIZARD" for define in CORE.defines)
    assert get_define_value("API_WIZARD_INPUT_COUNT") == "8"
    # Only ESP8266 copies text out of flash
    assert get_define_value("API_WIZARD_PAGE_SCRATCH_SIZE") is None


def test_inputs_have_buffers_hashes_and_default_filters(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))

    # The buffer holds the YAML entity ID until the wizard sets another
    assert 'static char api_wizard_input_ha_sensor[256] = "";' in main_cpp
    assert (
        'static char api_wizard_input_ha_default_sensor[256] = "sensor.default";'
        in main_cpp
    )
    for entity in (
        "ha_sensor",
        "ha_default_sensor",
        "ha_binary",
        "ha_text",
        "ha_number",
        "ha_switch",
    ):
        assert f"{entity}->set_entity_id(api_wizard_input_{entity});" in main_cpp
        assert f"{{{fnv1_hash(entity)}u, api_wizard_input_{entity}, " in main_cpp
    # Not an input, so it keeps its constant
    assert 'ha_plain_sensor->set_entity_id("sensor.plain");' in main_cpp
    # Without a target a switch accepts what the platform supports, and a number only numbers
    for domain in SWITCH_DOMAINS:
        assert f'"{domain}",' in main_cpp
    assert re.search(
        r'\{"", \{api_wizard_\d+, 1\}, \{nullptr, 0\}, \{nullptr, 0\}\},', main_cpp
    )
    assert main_cpp.count('"number",') == 2


def test_esp8266_keeps_text_in_progmem(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP8266_HEADER))

    # Equal strings are shared, and an unset one is nullptr
    assert main_cpp.count('PROGMEM = "sensor";') == 1
    assert re.search(
        r'static const char api_wizard_\d+\[\] PROGMEM = "Enable";', main_cpp
    )
    assert re.search(
        r"static const esphome::api::WizardEntityRow api_wizard_\d+\[\] PROGMEM = \{",
        main_cpp,
    )
    assert re.search(r"return sw; \}, api_wizard_\d+\},", main_cpp)
    assert "return sw; }, nullptr}," in main_cpp
    assert "static const char *const api_wizard_" in main_cpp
    assert "PROGMEM = {\n  api_wizard_" in main_cpp
    assert "{nullptr, {api_wizard_" in main_cpp
    # Room for the longest text of each kind, with its terminator: the page title and description
    # together, a field description, a filter integration and a list entry
    description = "[%key:component::domain::section::name%]"
    assert get_define_value("API_WIZARD_PAGE_SCRATCH_SIZE") == str(
        len("Setup") + len(description) + 2
    )
    assert get_define_value("API_WIZARD_FIELD_SCRATCH_SIZE") == str(len("Weather") + 1)
    assert get_define_value("API_WIZARD_FILTER_SCRATCH_SIZE") == str(len("met") + 1)
    assert get_define_value("API_WIZARD_LIST_SCRATCH_SIZE") == str(
        len("weather.WeatherEntityFeature.FORECAST_DAILY") + 1
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


def test_standalone_inputs_declare_an_object_with_a_buffer(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP32_HEADER))

    assert 'static char api_wizard_input_weather_input[256] = "";' in main_cpp
    assert 'static char api_wizard_input_unset_input[256] = "";' in main_cpp
    # The object reads the same buffer the wizard writes
    assert (
        re.search(
            r"weather_input = .*WizardInput\(api_wizard_input_weather_input\)", main_cpp
        )
        or "WizardInput(api_wizard_input_weather_input)" in main_cpp
    )
    assert f"{{{fnv1_hash('weather_input')}u, api_wizard_input_weather_input, " in (
        main_cpp
    )
    # A standalone input has no default filter, only the one it sets
    assert main_cpp.count('"weather",') == 1


def test_standalone_input_on_esp8266(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(write_input_config(tmp_path, ESP8266_HEADER))

    assert 'PROGMEM = "weather";' in main_cpp
    assert "WizardInput(api_wizard_input_unset_input)" in main_cpp


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
    "USE_API_WIZARD_ENTITIES",
    "USE_API_WIZARD_ENTITY_FILTERS",
    "USE_API_WIZARD_INPUTS",
    "USE_API_WIZARD_LINKED_INPUTS",
    "USE_API_WIZARD_STANDALONE_INPUTS",
}


@pytest.mark.parametrize(
    ("pages", "expected"),
    [
        pytest.param(
            "- entities: [{id: sw}]",
            {"USE_API_WIZARD", "USE_API_WIZARD_ENTITIES"},
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
                "USE_API_WIZARD_ENTITY_FILTERS",
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
                "USE_API_WIZARD_ENTITY_FILTERS",
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
                "USE_API_WIZARD_ENTITY_FILTERS",
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
    has_inputs = "USE_API_WIZARD_INPUTS" in expected
    assert (get_define_value("API_WIZARD_INPUT_COUNT") is not None) == has_inputs


def test_entity_rows_only_have_the_members_that_are_compiled_in(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(
        write_config(
            tmp_path, ESP32_HEADER, wizard_api("- title: T\n  entities: [{id: sw}]")
        )
    )

    assert re.search(r'\{"T", "", \{api_wizard_\d+, 1\}\}', main_cpp)


def test_input_rows_only_have_the_members_that_are_compiled_in(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main_cpp = generate_main(
        write_config(tmp_path, ESP32_HEADER, wizard_api("- inputs: [{id: standalone}]"))
    )

    assert re.search(r'\{\d+u, api_wizard_input_standalone, ""\}', main_cpp)
    assert re.search(r'\{"", "", \{api_wizard_\d+, 1\}\}', main_cpp)


def test_esp8266_without_filters_has_no_filter_scratch(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    generate_main(
        write_config(tmp_path, ESP8266_HEADER, wizard_api("- entities: [{id: sw}]"))
    )

    assert get_define_value("API_WIZARD_PAGE_SCRATCH_SIZE") is not None
    assert get_define_value("API_WIZARD_FIELD_SCRATCH_SIZE") is not None
    assert get_define_value("API_WIZARD_FILTER_SCRATCH_SIZE") is None
    assert get_define_value("API_WIZARD_LIST_SCRATCH_SIZE") is None
