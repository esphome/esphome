"""Config-validation tests for the sen6x sensor component."""

import pytest
from voluptuous import Invalid

from esphome.components.const import CONF_NOX_INDEX, CONF_VOC_INDEX
from esphome.components.sen6x.sensor import _deprecate_gas_index_keys
from esphome.const import CONF_NOX, CONF_VOC


@pytest.mark.parametrize(
    ("old_key", "new_key"),
    [(CONF_VOC, CONF_VOC_INDEX), (CONF_NOX, CONF_NOX_INDEX)],
)
def test_deprecated_key_remaps_to_index(old_key: str, new_key: str) -> None:
    """The deprecated voc/nox keys are remapped to voc_index/nox_index."""
    config = _deprecate_gas_index_keys({old_key: {"name": "test"}})
    assert old_key not in config
    assert config[new_key] == {"name": "test"}


@pytest.mark.parametrize(
    ("old_key", "new_key"),
    [(CONF_VOC, CONF_VOC_INDEX), (CONF_NOX, CONF_NOX_INDEX)],
)
def test_both_old_and_new_key_rejected(old_key: str, new_key: str) -> None:
    """Specifying both the deprecated and new key is an error."""
    with pytest.raises(Invalid, match="Cannot specify both"):
        _deprecate_gas_index_keys({old_key: {}, new_key: {}})


def test_new_keys_pass_through_unchanged() -> None:
    """A config using only the new keys is returned unchanged."""
    config = {CONF_VOC_INDEX: {"name": "voc"}, CONF_NOX_INDEX: {"name": "nox"}}
    assert _deprecate_gas_index_keys(config) == config
