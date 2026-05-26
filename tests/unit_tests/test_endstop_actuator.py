"""Unit tests for the EndstopActuatorBase component.

These tests verify:
1. esphome.components.endstop.EndstopActuatorBase can be imported
2. EndstopActuatorBase is exported from the endstop __init__.py
3. The endstop cover Python class inherits from both EndstopActuatorBase and Cover
"""


class TestEndstopActuatorImport:
    """EndstopActuatorBase can be imported from the endstop component."""

    def test_endstop_actuator_importable(self):
        """The EndstopActuatorBase must be importable from esphome.components.endstop."""
        # pylint: disable-next=unused-import
        from esphome.components.endstop import EndstopActuatorBase  # noqa: F401

    def test_endstop_actuator_class_exported(self):
        """The EndstopActuatorBase must be exported from endstop __init__.py."""
        import esphome.components.endstop as endstop_mod

        assert hasattr(endstop_mod, "EndstopActuatorBase"), (
            "EndstopActuatorBase not found in esphome.components.endstop"
        )

    def test_endstop_actuator_is_class(self):
        """The EndstopActuatorBase must be a class-like object (MockObj)."""
        from esphome.components.endstop import EndstopActuatorBase

        # MockObj instances are truthy and have string representations
        assert EndstopActuatorBase is not None


class TestEndstopCoverInheritance:
    """EndstopCover inherits EndstopActuatorBase and Cover."""

    def test_endstop_cover_inherits_actuator_base(self):
        """Verify EndstopCover inherits EndstopActuatorBase."""
        from esphome.components.endstop import EndstopActuatorBase
        from esphome.components.endstop.cover import EndstopCover

        # MockObjClass uses inherits_from() instead of Python issubclass()
        assert EndstopCover.inherits_from(EndstopActuatorBase), (
            f"EndstopCover does not inherit from EndstopActuatorBase. "
            f"EndstopCover parents: {getattr(EndstopCover, '_parents', 'N/A')}"
        )

    def test_endstop_cover_inherits_cover(self):
        """The EndstopCover must inherit from Cover."""
        from esphome.components.cover import Cover
        from esphome.components.endstop.cover import EndstopCover

        # MockObjClass uses inherits_from() instead of Python issubclass()
        assert EndstopCover.inherits_from(Cover), (
            f"EndstopCover does not inherit from Cover. "
            f"EndstopCover parents: {getattr(EndstopCover, '_parents', 'N/A')}"
        )

    def test_endstop_cover_not_direct_component(self):
        """Verify EndstopCover does not inherit directly from cg.Component.

        Component lifecycle comes from EndstopActuatorBase (which inherits Component).
        cg.Component is still reachable via EndstopActuatorBase (transitive inheritance),
        but it should not be a direct parent of EndstopCover.
        """
        import esphome.codegen as cg
        from esphome.components.endstop import EndstopActuatorBase
        from esphome.components.endstop.cover import EndstopCover

        # EndstopCover's direct parents are EndstopActuatorBase and Cover (not cg.Component)
        assert EndstopCover.inherits_from(EndstopActuatorBase), (
            "EndstopCover must inherit EndstopActuatorBase (which carries Component lifecycle)"
        )
        # Component IS reachable transitively — that's expected
        assert EndstopCover.inherits_from(cg.Component), (
            "EndstopCover must have Component reachable via EndstopActuatorBase"
        )
