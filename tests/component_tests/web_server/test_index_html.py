"""The generated index page and the js_extra_urls option that extends it."""

from pathlib import Path

import pytest

from esphome.components.web_server import CONFIG_SCHEMA, build_index_html
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

MAIN_SCRIPT = '<script src="https://oi.esphome.io/v3/www.js"></script>'


def _validated(
    set_core_config: SetCoreConfigCallable, config: ConfigType
) -> ConfigType:
    set_core_config(PlatformFramework.ESP32_IDF)
    return CONFIG_SCHEMA(config)


def test_extra_urls_follow_the_main_script(
    set_core_config: SetCoreConfigCallable,
) -> None:
    config = _validated(
        set_core_config,
        {
            "version": 3,
            "js_extra_urls": ["https://a.example/one.js", "https://b.example/two.js"],
        },
    )
    html = build_index_html(config)
    first = '<script type=module src="https://a.example/one.js"></script>'
    second = '<script type=module src="https://b.example/two.js"></script>'
    assert html.index("<esp-app>") < html.index(MAIN_SCRIPT) < html.index(first)
    assert html.index(first) < html.index(second) < html.index("</body>")


def test_without_extra_urls_the_page_is_unchanged(
    set_core_config: SetCoreConfigCallable,
) -> None:
    config = _validated(set_core_config, {"version": 3})
    assert "type=module" not in build_index_html(config)


def test_extra_urls_and_js_include_coexist(
    set_core_config: SetCoreConfigCallable, tmp_path: Path
) -> None:
    include = tmp_path / "local.js"
    include.write_text("// local")
    config = _validated(
        set_core_config,
        {
            "version": 3,
            "js_include": str(include),
            "js_extra_urls": ["https://a.example/one.js"],
        },
    )
    html = build_index_html(config)
    assert html.index("src=/0.js") < html.index("<esp-app>")
    assert html.index(MAIN_SCRIPT) < html.index("https://a.example/one.js")


def test_extra_urls_need_version_2_or_3(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="requires 'web_server' version 2 or 3"):
        _validated(
            set_core_config,
            {"version": 1, "js_extra_urls": ["https://a.example/one.js"]},
        )


@pytest.mark.parametrize("value", [[], ["not a url"], "https://a.example/one.js"])
def test_extra_urls_must_be_a_list_of_urls(
    set_core_config: SetCoreConfigCallable, value: object
) -> None:
    if isinstance(value, str):
        # A single string is accepted through ensure_list.
        config = _validated(set_core_config, {"version": 3, "js_extra_urls": value})
        assert config["js_extra_urls"] == [value]
        return
    with pytest.raises(cv.Invalid):
        _validated(set_core_config, {"version": 3, "js_extra_urls": value})
