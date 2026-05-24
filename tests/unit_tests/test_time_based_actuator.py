"""Unit tests for the TimeBasedActuatorBase component.

Phases:
  Phase 8: Tests must FAIL (cpp stub not yet populated, class not exported)
  Phase 9: Import tests pass (TimeBasedActuatorBase exported, cpp implemented)
  Phase 10: Inheritance test passes (TimeBasedCover inherits TimeBasedActuatorBase)
"""


def test_time_based_actuator_importable():
    from esphome.components.actuator import TimeBasedActuatorBase  # noqa: F401


def test_time_based_cover_inherits_actuator_base():
    """After Phase 10, TimeBasedCover must inherit TimeBasedActuatorBase."""
    from esphome.components.actuator import TimeBasedActuatorBase
    from esphome.components.cover import Cover
    from esphome.components.time_based.cover import TimeBasedCover

    # MockObjClass does not support Python's issubclass(); use inherits_from() instead
    assert TimeBasedCover.inherits_from(TimeBasedActuatorBase), (
        "TimeBasedCover does not inherit TimeBasedActuatorBase"
    )
    assert TimeBasedCover.inherits_from(Cover), "TimeBasedCover does not inherit Cover"


def test_time_based_actuator_class_exported():
    import esphome.components.actuator as actuator_mod

    assert hasattr(actuator_mod, "TimeBasedActuatorBase")
