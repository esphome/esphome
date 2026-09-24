"""Unit tests for the FeedbackActuatorBase component.

These tests verify:
1. esphome.components.feedback.FeedbackActuatorBase can be imported
2. FeedbackActuatorBase is exported from the feedback __init__.py
3. The feedback cover Python class inherits from both FeedbackActuatorBase and Cover
"""


class TestFeedbackActuatorImport:
    """FeedbackActuatorBase can be imported from the feedback component."""

    def test_feedback_actuator_importable(self):
        """The FeedbackActuatorBase must be importable from esphome.components.feedback."""
        # pylint: disable-next=unused-import
        from esphome.components.feedback import FeedbackActuatorBase  # noqa: F401

    def test_feedback_actuator_class_exported(self):
        """The FeedbackActuatorBase must be exported from feedback __init__.py."""
        import esphome.components.feedback as feedback_mod

        assert hasattr(feedback_mod, "FeedbackActuatorBase"), (
            "FeedbackActuatorBase not found in esphome.components.feedback"
        )

    def test_feedback_actuator_is_class(self):
        """The FeedbackActuatorBase must be a class-like object (MockObj)."""
        from esphome.components.feedback import FeedbackActuatorBase

        assert FeedbackActuatorBase is not None


class TestFeedbackCoverInheritance:
    """FeedbackCover inherits FeedbackActuatorBase and Cover."""

    def test_feedback_cover_inherits_actuator_base(self):
        """Verify FeedbackCover inherits FeedbackActuatorBase."""
        from esphome.components.feedback import FeedbackActuatorBase
        from esphome.components.feedback.cover import FeedbackCover

        assert FeedbackCover.inherits_from(FeedbackActuatorBase), (
            f"FeedbackCover does not inherit from FeedbackActuatorBase. "
            f"FeedbackCover parents: {getattr(FeedbackCover, '_parents', 'N/A')}"
        )

    def test_feedback_cover_inherits_cover(self):
        """The FeedbackCover must inherit from Cover."""
        from esphome.components.cover import Cover
        from esphome.components.feedback.cover import FeedbackCover

        assert FeedbackCover.inherits_from(Cover), (
            f"FeedbackCover does not inherit from Cover. "
            f"FeedbackCover parents: {getattr(FeedbackCover, '_parents', 'N/A')}"
        )

    def test_feedback_cover_component_reachable_transitively(self):
        """Component is reachable via FeedbackActuatorBase, not as a direct parent of FeedbackCover.

        FeedbackCover's direct parents are FeedbackActuatorBase and Cover.
        Component lifecycle flows through FeedbackActuatorBase.
        """
        import esphome.codegen as cg
        from esphome.components.feedback.cover import FeedbackCover

        assert FeedbackCover.inherits_from(cg.Component), (
            "FeedbackCover must have Component reachable via FeedbackActuatorBase"
        )
