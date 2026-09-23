"""The outer CONFIG_SCHEMA re-declares the esp32 scalar keys so tooling can walk
them without importing the esp32 BLE stack; pin the two declarations together.

The outer schema carries no defaults (the per-platform schema applies them), so
drift cannot surface in validation output — a key renamed or removed in
_esp32_config_schema() but not here would silently vanish from the dashboard's
field extractor. This test is what catches that. The outer schema bounds
connection_slots with the loosest platform cap (_IDF_MAX_CONNECTIONS) so range
walkers see a real Range; per-platform schemas tighten it, and the cap itself
is pinned by test_idf_max_connections_mirror.
"""

import voluptuous as vol

from esphome import config_validation as cv
from esphome.components.bluetooth_proxy import CONFIG_SCHEMA, _esp32_config_schema


def _esp32_schema_keys() -> dict[str, object]:
    # The builder names its platform explicitly, so no CORE state is needed
    # (this also mirrors how the language-schema dumper calls it).
    return _keys(_schema_of(_esp32_config_schema()))


# esp32-schema keys with no place in the outer schema: COMPONENT_SCHEMA
# plumbing (derived, so a future core key does not fail this component's test),
# generated IDs (not user-walkable options), and connections (must validate
# exactly once — see the comment above CONFIG_SCHEMA).
_NOT_MIRRORED = {str(key.schema) for key in cv.COMPONENT_SCHEMA.schema} | {
    "connections"
}


def _schema_of(validator: cv.All) -> vol.Schema:
    """The vol.Schema stage of a cv.All chain, found by type rather than by
    position so reordering the chain cannot silently break these tests."""
    schemas = [v for v in validator.validators if isinstance(v, vol.Schema)]
    assert len(schemas) == 1, f"expected exactly one vol.Schema stage, got {schemas}"
    return schemas[0]


def _keys(schema: vol.Schema) -> dict[str, object]:
    return {str(key.schema): key for key in schema.schema}


def test_outer_scalar_keys_exist_in_esp32_schema() -> None:
    outer = _keys(_schema_of(CONFIG_SCHEMA))
    esp32 = _esp32_schema_keys()
    missing = set(outer) - set(esp32)
    assert not missing, (
        f"outer CONFIG_SCHEMA declares {sorted(missing)} which the esp32 schema "
        "does not; update one of them in "
        "esphome/components/bluetooth_proxy/__init__.py"
    )


def test_esp32_scalars_all_walkable() -> None:
    """Every non-generated esp32 scalar option must appear in the outer schema
    (connections is deliberately excluded — it must validate exactly once)."""
    outer = _keys(_schema_of(CONFIG_SCHEMA))
    esp32 = _esp32_schema_keys()
    scalar = {
        name
        for name, key in esp32.items()
        if isinstance(key, vol.Optional)
        and not isinstance(key, cv.GenerateID)
        and name not in _NOT_MIRRORED
    }
    missing = scalar - set(outer)
    assert not missing, (
        f"esp32 scalar options {sorted(missing)} are missing from the outer "
        "CONFIG_SCHEMA and invisible to schema tooling; update "
        "esphome/components/bluetooth_proxy/__init__.py"
    )
