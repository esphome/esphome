"""Unit tests for the actuator base component.

These tests verify:
1. The actuator module can be imported
2. Constants ACTUATOR_OPEN and ACTUATOR_CLOSED exist with correct values
3. ActuatorOperation enum is accessible with correct members
4. Cover and Valve use actuator.ActuatorOperation (backward compat)
5. Python-level class relationships after migration

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

    def test_actuator_open_constant_exists(self):
        """ACTUATOR_OPEN constant must exist in the actuator module."""
        from esphome.components import actuator

        assert hasattr(actuator, "ACTUATOR_OPEN"), (
            "ACTUATOR_OPEN not found in actuator module"
        )

    def test_actuator_closed_constant_exists(self):
        """ACTUATOR_CLOSED constant must exist in the actuator module."""
        from esphome.components import actuator

        assert hasattr(actuator, "ACTUATOR_CLOSED"), (
            "ACTUATOR_CLOSED not found in actuator module"
        )

    def test_actuator_open_value(self):
        """ACTUATOR_OPEN must resolve to 1.0f (string representation '1.0f')."""
        from esphome.components import actuator

        val = actuator.ACTUATOR_OPEN
        # The value is a MockObj referencing the C++ constexpr; check its string repr
        assert str(val) == "1.0f", f"ACTUATOR_OPEN expected '1.0f', got '{val}'"

    def test_actuator_closed_value(self):
        """ACTUATOR_CLOSED must resolve to 0.0f (string representation '0.0f')."""
        from esphome.components import actuator

        val = actuator.ACTUATOR_CLOSED
        # The value is a MockObj referencing the C++ constexpr; check its string repr
        assert str(val) == "0.0f", f"ACTUATOR_CLOSED expected '0.0f', got '{val}'"

    def test_actuator_operation_enum_accessible(self):
        """The ActuatorOperation enum must be accessible from the actuator module."""
        from esphome.components import actuator

        assert hasattr(actuator, "ActuatorOperation"), (
            "ActuatorOperation not found in actuator module"
        )

    def test_actuator_operation_idle(self):
        """ActuatorOperation.ACTUATOR_OPERATION_IDLE must render as the qualified C++ name."""
        from esphome.components import actuator

        op = actuator.ActuatorOperation
        idle = op.ACTUATOR_OPERATION_IDLE
        assert str(idle) == "actuator::ACTUATOR_OPERATION_IDLE", (
            f"ACTUATOR_OPERATION_IDLE expected qualified C++ name, got '{idle}'"
        )

    def test_actuator_operation_opening(self):
        """ActuatorOperation.ACTUATOR_OPERATION_OPENING must render as the qualified C++ name."""
        from esphome.components import actuator

        op = actuator.ActuatorOperation
        opening = op.ACTUATOR_OPERATION_OPENING
        assert str(opening) == "actuator::ACTUATOR_OPERATION_OPENING", (
            f"ACTUATOR_OPERATION_OPENING expected qualified C++ name, got '{opening}'"
        )

    def test_actuator_operation_closing(self):
        """ActuatorOperation.ACTUATOR_OPERATION_CLOSING must render as the qualified C++ name."""
        from esphome.components import actuator

        op = actuator.ActuatorOperation
        closing = op.ACTUATOR_OPERATION_CLOSING
        assert str(closing) == "actuator::ACTUATOR_OPERATION_CLOSING", (
            f"ACTUATOR_OPERATION_CLOSING expected qualified C++ name, got '{closing}'"
        )

    def test_actuator_base_class_exists(self):
        """The ActuatorBase class must exist in the actuator module."""
        from esphome.components import actuator

        assert hasattr(actuator, "ActuatorBase"), (
            "ActuatorBase not found in actuator module"
        )


class TestCoverActuatorBackwardCompat:
    """Phase 4+: Cover uses actuator.ActuatorOperation (backward compat alias)."""

    def test_cover_imports_actuator(self):
        """The cover module must depend on actuator (DEPENDENCIES contains 'actuator')."""
        import esphome.components.cover as cover_mod

        deps = getattr(cover_mod, "DEPENDENCIES", [])
        assert "actuator" in deps, f"'actuator' not in cover DEPENDENCIES: {deps}"

    def test_cover_operation_is_actuator_operation(self):
        """Cover IDLE operation value must match ActuatorOperation IDLE integer value."""
        from esphome.components import actuator
        import esphome.components.cover as cover_mod

        cover_idle = cover_mod.COVER_OPERATIONS["IDLE"]
        actuator_idle = actuator.ActuatorOperation.ACTUATOR_OPERATION_IDLE
        assert str(cover_idle) == str(actuator_idle), (
            f"COVER_OPERATION_IDLE ({cover_idle}) != ACTUATOR_OPERATION_IDLE ({actuator_idle})"
        )


class TestValveActuatorBackwardCompat:
    """Phase 4+: Valve uses actuator.ActuatorOperation."""

    def test_valve_imports_actuator(self):
        """The valve module must depend on actuator (DEPENDENCIES contains 'actuator')."""
        import esphome.components.valve as valve_mod

        deps = getattr(valve_mod, "DEPENDENCIES", [])
        assert "actuator" in deps, f"'actuator' not in valve DEPENDENCIES: {deps}"

    def test_valve_operation_is_actuator_operation(self):
        """Valve IDLE operation value must match ActuatorOperation IDLE integer value."""
        from esphome.components import actuator
        import esphome.components.valve as valve_mod

        # Verify ValveOperation members match actuator enum integer values
        valve_idle = valve_mod.VALVE_OPERATIONS["IDLE"]
        actuator_idle = actuator.ActuatorOperation.ACTUATOR_OPERATION_IDLE
        assert str(valve_idle) == str(actuator_idle), (
            f"VALVE_OPERATION_IDLE ({valve_idle}) != ACTUATOR_OPERATION_IDLE ({actuator_idle})"
        )
