"""Unit tests for the TimeBasedActuatorBase component."""


def test_time_based_actuator_importable():
    # pylint: disable-next=unused-import
    from esphome.components.time_based import TimeBasedActuatorBase  # noqa: F401


def test_time_based_cover_inherits_actuator_base():
    """Verify TimeBasedCover inherits TimeBasedActuatorBase."""
    from esphome.components.cover import Cover
    from esphome.components.time_based import TimeBasedActuatorBase
    from esphome.components.time_based.cover import TimeBasedCover

    # MockObjClass does not support Python's issubclass(); use inherits_from() instead
    assert TimeBasedCover.inherits_from(TimeBasedActuatorBase), (
        "TimeBasedCover does not inherit TimeBasedActuatorBase"
    )
    assert TimeBasedCover.inherits_from(Cover), "TimeBasedCover does not inherit Cover"


def test_time_based_actuator_class_exported():
    import esphome.components.time_based as time_based_mod

    assert hasattr(time_based_mod, "TimeBasedActuatorBase")
