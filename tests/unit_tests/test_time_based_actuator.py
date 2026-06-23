"""Unit tests for the TimeBasedActuatorBase component.

These tests verify:
1. esphome.components.time_based.TimeBasedActuatorBase can be imported
2. TimeBasedActuatorBase is exported from the time_based __init__.py
3. The time_based cover Python class inherits from both TimeBasedActuatorBase and Cover
"""


class TestTimeBasedActuatorImport:
    """TimeBasedActuatorBase can be imported from the time_based component."""

    def test_time_based_actuator_importable(self):
        """The TimeBasedActuatorBase must be importable from esphome.components.time_based."""
        # pylint: disable-next=unused-import
        from esphome.components.time_based import TimeBasedActuatorBase  # noqa: F401

    def test_time_based_actuator_class_exported(self):
        """The TimeBasedActuatorBase must be exported from time_based __init__.py."""
        import esphome.components.time_based as time_based_mod

        assert hasattr(time_based_mod, "TimeBasedActuatorBase"), (
            "TimeBasedActuatorBase not found in esphome.components.time_based"
        )

    def test_time_based_actuator_is_class(self):
        """The TimeBasedActuatorBase must be a class-like object (MockObj)."""
        from esphome.components.time_based import TimeBasedActuatorBase

        assert TimeBasedActuatorBase is not None


class TestTimeBasedCoverInheritance:
    """TimeBasedCover inherits TimeBasedActuatorBase and Cover."""

    def test_time_based_cover_inherits_actuator_base(self):
        """Verify TimeBasedCover inherits TimeBasedActuatorBase."""
        from esphome.components.time_based import TimeBasedActuatorBase
        from esphome.components.time_based.cover import TimeBasedCover

        assert TimeBasedCover.inherits_from(TimeBasedActuatorBase), (
            f"TimeBasedCover does not inherit from TimeBasedActuatorBase. "
            f"TimeBasedCover parents: {getattr(TimeBasedCover, '_parents', 'N/A')}"
        )

    def test_time_based_cover_inherits_cover(self):
        """The TimeBasedCover must inherit from Cover."""
        from esphome.components.cover import Cover
        from esphome.components.time_based.cover import TimeBasedCover

        assert TimeBasedCover.inherits_from(Cover), (
            f"TimeBasedCover does not inherit from Cover. "
            f"TimeBasedCover parents: {getattr(TimeBasedCover, '_parents', 'N/A')}"
        )

    def test_time_based_cover_component_reachable_transitively(self):
        """Component is reachable via TimeBasedActuatorBase, not as a direct parent of TimeBasedCover.

        TimeBasedCover's direct parents are TimeBasedActuatorBase and Cover.
        Component lifecycle flows through TimeBasedActuatorBase.
        """
        import esphome.codegen as cg
        from esphome.components.time_based.cover import TimeBasedCover

        assert TimeBasedCover.inherits_from(cg.Component), (
            "TimeBasedCover must have Component reachable via TimeBasedActuatorBase"
        )
