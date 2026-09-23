"""Unit tests for the actuator base component.

These tests verify:
1. The actuator module can be imported
2. Cover and Valve auto-load actuator
3. Cover and Valve operation constants keep their domain names (backward compat)

Phases:
  Phase 2: Tests must FAIL (actuator module does not exist yet)
  Phase 3: Import tests pass
  Phase 4: All tests pass (Cover/Valve migrated)
"""


class TestActuatorModuleImport:
    """Phase 3+: actuator module can be imported."""

    def test_actuator_module_importable(self):
        """The esphome.components.actuator module must be importable."""
        # pylint: disable-next=unused-import
        from esphome.components import actuator  # noqa: F401

    def test_actuator_base_class_exists(self):
        """The ActuatorBase class must exist in the actuator module."""
        from esphome.components import actuator

        assert hasattr(actuator, "ActuatorBase"), (
            "ActuatorBase not found in actuator module"
        )


class TestCoverActuatorBackwardCompat:
    """Phase 4+: Cover uses actuator.ActuatorOperation (backward compat alias)."""

    def test_cover_imports_actuator(self):
        """The cover module must auto-load actuator."""
        import esphome.components.cover as cover_mod

        assert "actuator" in cover_mod.AUTO_LOAD

    def test_cover_operations_render_as_cover_constants(self):
        """COVER_OPERATIONS must render as the cover-namespaced backward-compat constants."""
        import esphome.components.cover as cover_mod

        assert str(cover_mod.COVER_OPERATIONS["IDLE"]) == "cover::COVER_OPERATION_IDLE"
        assert (
            str(cover_mod.COVER_OPERATIONS["OPENING"])
            == "cover::COVER_OPERATION_OPENING"
        )
        assert (
            str(cover_mod.COVER_OPERATIONS["CLOSING"])
            == "cover::COVER_OPERATION_CLOSING"
        )


class TestValveActuatorBackwardCompat:
    """Phase 4+: Valve uses actuator.ActuatorOperation."""

    def test_valve_imports_actuator(self):
        """The valve module must auto-load actuator."""
        import esphome.components.valve as valve_mod

        assert "actuator" in valve_mod.AUTO_LOAD

    def test_valve_operations_render_as_valve_constants(self):
        """VALVE_OPERATIONS must render as the valve-namespaced backward-compat constants."""
        import esphome.components.valve as valve_mod

        assert str(valve_mod.VALVE_OPERATIONS["IDLE"]) == "valve::VALVE_OPERATION_IDLE"
        assert (
            str(valve_mod.VALVE_OPERATIONS["OPENING"])
            == "valve::VALVE_OPERATION_OPENING"
        )
        assert (
            str(valve_mod.VALVE_OPERATIONS["CLOSING"])
            == "valve::VALVE_OPERATION_CLOSING"
        )
