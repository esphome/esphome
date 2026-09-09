"""Tests for esphome.config.IDPassValidationStep's handling of a searching id whose
`type` is a tuple of acceptable alternatives, rather than a single type.

This is what `entity_state:` relies on (see esphome/config_validation.py) to reject a
reference to a declared id whose type has no usable `.state` field, and what narrows
the message to a single type name when the field's own validator (e.g. `cv.string`)
narrows the acceptable set down to one.
"""

from __future__ import annotations

from esphome.components.binary_sensor import BinarySensor
from esphome.components.number import Number
from esphome.components.sensor import Sensor
from esphome.components.text_sensor import TextSensor
from esphome.config import Config, IDPassValidationStep
from esphome.core import ID, Lambda


def _run_id_pass(declared_type, explicit_id_types: tuple) -> Config:
    config = Config()
    config["sensor"] = [{"id": ID("s1", is_declaration=True, type=declared_type)}]
    lambda_ = Lambda("return id(s1).state;")
    lambda_.set_requires_ids([ID("s1", is_declaration=False, type=explicit_id_types)])
    config["text_sensor"] = [{"text": lambda_}]
    IDPassValidationStep().run(config)
    return config


def test_id_pass_rejects_wrong_type_against_single_allowed_type() -> None:
    """A searching id's `type` narrowed to exactly one option (as
    `_entity_state_allowed_types` does once the field's own validator narrows the
    acceptable set, e.g. to just `TextSensor` for a string field) must produce the
    singular "doesn't inherit from" message, not the plural "is not one of" one.
    """
    config = _run_id_pass(declared_type=Sensor, explicit_id_types=(TextSensor,))

    assert config.errors
    assert any(
        "doesn't inherit from" in str(err) and "is not one of" not in str(err)
        for err in config.errors
    )


def test_id_pass_rejects_wrong_type_against_multiple_allowed_types() -> None:
    """A searching id's `type` with several acceptable alternatives (the default,
    unnarrowed `entity_state:` case) must produce the plural "is not one of" message.
    """
    config = _run_id_pass(
        declared_type=BinarySensor, explicit_id_types=(TextSensor, Number)
    )

    assert config.errors
    assert any("is not one of" in str(err) for err in config.errors)


def test_id_pass_accepts_matching_type_in_tuple() -> None:
    """No error when the declared id's type is one of the accepted alternatives."""
    config = _run_id_pass(declared_type=Sensor, explicit_id_types=(TextSensor, Sensor))

    assert not config.errors


def test_id_pass_reports_missing_id_once() -> None:
    """Regression: `entity_state:` used to attach a typed id via a separate
    `explicit_ids` list on top of the untyped one `Lambda.requires_ids` parses out of
    the generated source, so a nonexistent id was reported twice. `set_requires_ids`
    replaces the parsed id instead of adding to it, so there is exactly one id here
    and exactly one error.
    """
    config = Config()
    lambda_ = Lambda("return id(missing).state;")
    lambda_.set_requires_ids([ID("missing", is_declaration=False, type=(Sensor,))])
    config["text_sensor"] = [{"text": lambda_}]
    IDPassValidationStep().run(config)

    assert sum("Couldn't find ID 'missing'" in str(err) for err in config.errors) == 1
