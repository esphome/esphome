"""The voluptuous compatibility shim for third-party (external) components.

External components import ``voluptuous`` directly. ESPHome installs probatio's
shim at package import (``esphome/__init__.py``), so those imports resolve to
probatio without the components needing any change. These tests pin that contract.
"""

import sys

import probatio

import esphome  # noqa: F401  (importing installs the shim)


def test_voluptuous_resolves_to_probatio_shim() -> None:
    """``import voluptuous`` resolves to probatio's shim, not a real voluptuous."""
    voluptuous = sys.modules.get("voluptuous")
    assert voluptuous is not None
    assert voluptuous.__name__ == "probatio._vol_shim"
    # Submodules dependencies reach into are aliased too.
    assert "voluptuous.schema_builder" in sys.modules
    assert hasattr(sys.modules["voluptuous.schema_builder"], "_compile_scalar")


def test_external_component_style_schema_validates() -> None:
    """A schema built the way a third-party component would, through voluptuous."""
    import voluptuous as vol  # noqa: PLC0415  (mimics an external component)

    # Markers imported from voluptuous are probatio markers via the shim.
    assert issubclass(vol.Required, probatio.Marker)

    schema = vol.Schema(
        {
            vol.Required("name"): str,
            vol.Optional("count", default=1): int,
        }
    )
    assert schema({"name": "x"}) == {"name": "x", "count": 1}


def test_shim_extra_key_is_probatio_error() -> None:
    """An extra key raises probatio's MultipleInvalid through the shim."""
    import voluptuous as vol  # noqa: PLC0415

    schema = vol.Schema({vol.Required("name"): str})
    try:
        schema({"name": "x", "bogus": 1})
    except vol.MultipleInvalid as err:
        assert isinstance(err, probatio.MultipleInvalid)
    else:
        raise AssertionError("expected MultipleInvalid for an extra key")
