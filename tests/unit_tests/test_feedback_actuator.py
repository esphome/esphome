"""Unit tests for the FeedbackActuatorBase component."""


def test_feedback_actuator_importable():
    # pylint: disable-next=unused-import
    from esphome.components.feedback import FeedbackActuatorBase  # noqa: F401


def test_feedback_cover_inherits_actuator_base():
    """Verify FeedbackCover inherits FeedbackActuatorBase."""
    from esphome.components.cover import Cover
    from esphome.components.feedback import FeedbackActuatorBase
    from esphome.components.feedback.cover import FeedbackCover

    # MockObjClass does not support Python's issubclass(); use inherits_from() instead
    assert FeedbackCover.inherits_from(FeedbackActuatorBase), (
        "FeedbackCover does not inherit FeedbackActuatorBase"
    )
    assert FeedbackCover.inherits_from(Cover), "FeedbackCover does not inherit Cover"


def test_feedback_actuator_class_exported():
    import esphome.components.feedback as feedback_mod

    assert hasattr(feedback_mod, "FeedbackActuatorBase")
