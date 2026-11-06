"""Tests for the web_server resources emitted as flash tables."""

from collections.abc import Callable
import gzip
from pathlib import Path
import re

import pytest
import yaml

from esphome.components import web_server

try:
    import brotlicffi as brotli
except ImportError:
    import brotli


def _table(main_cpp: str, name: str) -> bytes:
    # Anchored so a static table does not match
    match = re.search(
        rf"^const uint8_t ESPHOME_WEBSERVER_{name}\[\] PROGMEM = \{{([^}}]*)\}};",
        main_cpp,
        re.MULTILINE,
    )
    assert match is not None
    return bytes(int(byte) for byte in match.group(1).split(","))


def test_resources_are_externally_linked_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """web_server.cpp reads these through extern declarations, so they must not be static."""
    main_cpp = generate_main(component_config_path("includes.yaml"))

    assert b"<esp-app></esp-app>" in _table(main_cpp, "INDEX_HTML")
    assert gzip.decompress(_table(main_cpp, "CSS_INCLUDE")) == b"body { color: red; }\n"
    assert gzip.decompress(_table(main_cpp, "JS_INCLUDE")) == b'console.log("hi");\n'
    css_size = len(_table(main_cpp, "CSS_INCLUDE"))
    assert (
        f"constexpr size_t ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE = {css_size};" in main_cpp
    )


@pytest.mark.parametrize("version", [2, 3])
@pytest.mark.parametrize("compression", ["gzip", "br"])
@pytest.mark.parametrize("explicit_local", [True, False], ids=["local", "ap-only"])
def test_local_page_loads_js_include(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
    version: int,
    compression: str,
    explicit_local: bool,
) -> None:
    """Custom modules run before the embedded interface, including in AP-only mode."""
    config = yaml.safe_load(component_config_path("includes.yaml").read_text())
    config["web_server"] = {
        "version": version,
        "compression": compression,
        "js_include": "include.js",
    }
    if explicit_local:
        config["web_server"]["local"] = True
    else:
        config["wifi"] = {"ap": {"ssid": "Local test"}}
    (tmp_path / "include.js").write_text("window.customScriptLoaded = true;\n")
    config_path = tmp_path / "local.yaml"
    config_path.write_text(yaml.safe_dump(config))

    main_cpp = generate_main(config_path)
    index = _table(main_cpp, "INDEX_HTML")
    decompress = gzip.decompress if compression == "gzip" else brotli.decompress
    html = decompress(index).decode()

    custom_script = "<script type=module src=/0.js></script>"
    assert html.count(custom_script) == 1
    source = (
        Path(web_server.__file__)
        .with_name(f"server_index_v{version}.h")
        .read_text(encoding="utf-8")
    )
    bundled = re.search(r"INDEX_GZ\[\] PROGMEM = \{([^}]+)\}", source)
    assert bundled is not None
    original = gzip.decompress(
        bytes(int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", bundled[1]))
    ).decode()
    assert html.replace(custom_script, "", 1) == original
    assert f"ESPHOME_WEBSERVER_INDEX_HTML_SIZE = {len(index)};" in main_cpp
    assert html.index(custom_script) < html.index("<script type=module crossorigin>")
    assert "<esp-app></esp-app>" in html
    assert "https://oi.esphome.io" not in html
    assert gzip.decompress(_table(main_cpp, "JS_INCLUDE")) == (
        b"window.customScriptLoaded = true;\n"
    )


@pytest.mark.parametrize("version", [2, 3])
def test_local_page_without_js_include_uses_bundled_asset(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
    version: int,
) -> None:
    """The existing static page remains available when there is no custom module."""
    config = yaml.safe_load(component_config_path("includes.yaml").read_text())
    config["web_server"] = {"version": version, "local": True}
    config_path = tmp_path / "local.yaml"
    config_path.write_text(yaml.safe_dump(config))

    main_cpp = generate_main(config_path)
    html = _table(main_cpp, "INDEX_HTML").decode()
    assert "/0.js" not in html
    assert "ESPHOME_WEBSERVER_JS_INCLUDE[]" not in main_cpp


@pytest.mark.parametrize("version", [2, 3])
def test_ap_with_local_disabled_keeps_hosted_js_include(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
    version: int,
) -> None:
    """Explicit local: false still loads the include from the hosted-page shell."""
    config = yaml.safe_load(component_config_path("includes.yaml").read_text())
    config["wifi"] = {"ap": {"ssid": "Local test"}}
    config["web_server"] = {
        "version": version,
        "local": False,
        "js_include": str(component_config_path("include.js")),
    }
    config_path = tmp_path / "hosted.yaml"
    config_path.write_text(yaml.safe_dump(config))

    html = _table(generate_main(config_path), "INDEX_HTML").decode()
    assert "<script type=module src=/0.js></script>" in html
    assert f"https://oi.esphome.io/v{version}/www.js" in html
