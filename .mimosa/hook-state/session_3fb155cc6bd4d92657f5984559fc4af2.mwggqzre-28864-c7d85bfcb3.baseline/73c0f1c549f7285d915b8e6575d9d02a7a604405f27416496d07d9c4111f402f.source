"""Tests for __pstorage symbol attribution in memory analyzer."""

from esphome.analyze_memory import _PSTORAGE_SUFFIX

from . import make_analyzer


def test_pstorage_suffix_constant() -> None:
    """Verify the suffix constant matches what codegen produces."""
    assert _PSTORAGE_SUFFIX == "__pstorage"


def test_match_pstorage_simple_component() -> None:
    """Simple component name like 'logger'."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("logger__logger_id__pstorage")
    assert result == "[esphome]logger"


def test_match_pstorage_underscore_component() -> None:
    """Component with underscore like 'web_server'."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("web_server__webserver_id__pstorage")
    assert result == "[esphome]web_server"


def test_match_pstorage_api() -> None:
    """API component."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("api__apiserver_id__pstorage")
    assert result == "[esphome]api"


def test_match_pstorage_deep_sleep() -> None:
    """Component with underscore: deep_sleep."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("deep_sleep__deepsleep__pstorage")
    assert result == "[esphome]deep_sleep"


def test_match_pstorage_status_led() -> None:
    """Component with underscore: status_led."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("status_led__statusled_id__pstorage")
    assert result == "[esphome]status_led"


def test_match_pstorage_external_component() -> None:
    """External component should be attributed correctly."""
    analyzer = make_analyzer(external_components={"my_custom"})
    result = analyzer._match_pstorage_component("my_custom__thing_id__pstorage")
    assert result == "[external]my_custom"


def test_match_pstorage_no_dunder_returns_none() -> None:
    """Symbol without double underscore separator returns None."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("something__pstorage")
    assert result is None


def test_match_pstorage_unknown_component_returns_none() -> None:
    """Unknown component namespace returns None."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("nonexistent__thing_id__pstorage")
    assert result is None


def test_match_pstorage_esphome_component() -> None:
    """esphome:: namespace types map to the esphome component."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component(
        "esphome__esphomeotacomponent_id__pstorage"
    )
    assert result == "[esphome]esphome"


def test_match_pstorage_user_id_with_component_prefix() -> None:
    """User-chosen ID that happens to contain a component name."""
    analyzer = make_analyzer()
    result = analyzer._match_pstorage_component("logger__relay1__pstorage")
    assert result == "[esphome]logger"
