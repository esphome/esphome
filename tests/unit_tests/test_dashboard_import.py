"""Unit tests for ``esphome.components.dashboard_import.import_config``.

Locks the YAML shape that ``import_config`` materialises on disk for
adopted factory firmware. Both the legacy dashboard and the new
device-builder backend (esphome/device-builder) call this function
during the adoption flow and depend on the output's ``esphome.name``
/ ``packages:`` keys to route subsequent compile + flash operations.
"""

from __future__ import annotations

from pathlib import Path

import pytest
import yaml as pyyaml

from esphome.components.dashboard_import import import_config


def _load_plain_yaml(path: Path) -> dict:
    """Load YAML without invoking ESPHome's ``CORE``-aware loader.

    ``esphome.yaml_util.load_yaml`` resolves ``!include`` /
    ``!secret`` against ``CORE.config_path`` which isn't set in
    these tests. We're only asserting on plain key/value structure,
    so ``pyyaml.load`` with a custom loader subclassing
    ``pyyaml.SafeLoader`` (and empty fallbacks for the secret/include
    tags) is enough.
    """

    class _Loader(pyyaml.SafeLoader):
        pass

    _Loader.add_constructor("!secret", lambda loader, node: f"!secret {node.value}")
    _Loader.add_constructor("!include", lambda loader, node: f"!include {node.value}")

    return pyyaml.load(path.read_text(encoding="utf-8"), Loader=_Loader)


def test_basic_import_writes_expected_yaml_shape(tmp_path: Path) -> None:
    """A minimal Wi-Fi import emits the substitutions / packages / esphome triad.

    These three top-level blocks are the contract: substitutions
    holds the device-specific name, packages pulls in the upstream
    firmware via the import URL, and esphome.name interpolates from
    substitutions. Anything that depends on this output (frontend
    config viewer, follow-up edits, version checks) reads those
    keys directly.
    """
    yaml_path = tmp_path / "kitchen.yaml"

    import_config(
        path=str(yaml_path),
        name="kitchen",
        friendly_name="Kitchen",
        project_name="acme.kitchen-light",
        import_url="github://acme/firmware/kitchen.yaml@main",
    )

    assert yaml_path.exists()
    config = _load_plain_yaml(yaml_path)

    assert config["substitutions"] == {
        "name": "kitchen",
        "friendly_name": "Kitchen",
    }
    assert config["packages"] == {
        "acme.kitchen-light": "github://acme/firmware/kitchen.yaml@main"
    }
    assert config["esphome"] == {
        "name": "${name}",
        "name_add_mac_suffix": False,
        "friendly_name": "${friendly_name}",
    }


def test_import_appends_wifi_config_when_network_is_wifi(tmp_path: Path) -> None:
    """Wi-Fi devices get a ``wifi:`` block templated with secrets references.

    Adopted Wi-Fi devices need a ``wifi:`` section so they can
    actually connect on the user's LAN — the boilerplate references
    ``!secret wifi_ssid`` / ``!secret wifi_password`` so the
    user's existing secrets file plugs in. Devices on other
    networks (Ethernet) shouldn't get the Wi-Fi block.
    """
    yaml_path = tmp_path / "kitchen.yaml"
    import_config(
        path=str(yaml_path),
        name="kitchen",
        friendly_name=None,
        project_name="acme.kitchen-light",
        import_url="github://acme/firmware/kitchen.yaml@main",
    )
    contents = yaml_path.read_text()
    assert "wifi:" in contents
    assert "!secret wifi_ssid" in contents
    assert "!secret wifi_password" in contents


def test_import_omits_wifi_block_for_ethernet_network(tmp_path: Path) -> None:
    """Ethernet devices get no ``wifi:`` block — caller wires Ethernet separately.

    The ``network`` parameter exists specifically so non-Wi-Fi
    devices (PoE / Ethernet, etc.) skip the Wi-Fi templating —
    otherwise their generated YAML would carry an unused ``wifi:``
    section the user has to clean up by hand.
    """
    yaml_path = tmp_path / "olimex-poe.yaml"
    import_config(
        path=str(yaml_path),
        name="olimex-poe",
        friendly_name=None,
        project_name="acme.poe-monitor",
        import_url="github://acme/firmware/poe.yaml@main",
        network="ethernet",
    )
    contents = yaml_path.read_text()
    assert "wifi:" not in contents


def test_import_with_encryption_writes_api_key(tmp_path: Path) -> None:
    """``encryption=True`` generates a fresh Noise PSK in the api block.

    Used during the adoption flow when the device-builder UI
    explicitly opts the new device into encrypted API. Each
    invocation must produce a fresh 32-byte PSK base64-encoded into
    the YAML; subsequent compiles and the dashboard's encryption
    indicator both read it from there.
    """
    yaml_path_1 = tmp_path / "a.yaml"
    yaml_path_2 = tmp_path / "b.yaml"

    import_config(
        path=str(yaml_path_1),
        name="a",
        friendly_name=None,
        project_name="acme.dev",
        import_url="github://acme/firmware/dev.yaml@main",
        encryption=True,
    )
    import_config(
        path=str(yaml_path_2),
        name="b",
        friendly_name=None,
        project_name="acme.dev",
        import_url="github://acme/firmware/dev.yaml@main",
        encryption=True,
    )

    config_1 = _load_plain_yaml(yaml_path_1)
    config_2 = _load_plain_yaml(yaml_path_2)
    assert "api" in config_1 and "encryption" in config_1["api"]
    key_1 = config_1["api"]["encryption"]["key"]
    key_2 = config_2["api"]["encryption"]["key"]
    # Fresh per-call PSK, not a hardcoded value.
    assert key_1 != key_2
    # Base64-encoded 32 bytes → length 44 with one trailing `=`.
    assert len(key_1) == 44


def test_import_without_friendly_name_omits_friendly_substitution(
    tmp_path: Path,
) -> None:
    """``friendly_name=None`` skips the friendly_name substitution.

    Some imported configs don't carry a friendly name. The output
    shouldn't pretend they do — the substitutions block must omit
    ``friendly_name`` so the dashboard renders blank rather than
    the literal substitution token.
    """
    yaml_path = tmp_path / "noname.yaml"
    import_config(
        path=str(yaml_path),
        name="noname",
        friendly_name=None,
        project_name="acme.dev",
        import_url="github://acme/firmware/dev.yaml@main",
    )
    config = _load_plain_yaml(yaml_path)
    assert config["substitutions"] == {"name": "noname"}
    assert "friendly_name" not in config["esphome"]


def test_import_refuses_to_overwrite_existing_yaml(tmp_path: Path) -> None:
    """An already-present file raises rather than clobbering the user's edits.

    Both the legacy dashboard and device-builder rely on the
    ``FileExistsError`` to surface a "config already exists" message
    instead of silently destroying user data.
    """
    yaml_path = tmp_path / "existing.yaml"
    yaml_path.write_text("# user's hand-edited config\n", encoding="utf-8")

    with pytest.raises(FileExistsError):
        import_config(
            path=str(yaml_path),
            name="existing",
            friendly_name=None,
            project_name="acme.dev",
            import_url="github://acme/firmware/dev.yaml@main",
        )
    # Original content survives unchanged.
    assert yaml_path.read_text() == "# user's hand-edited config\n"
