"""Unit tests for esphome.automation module."""

from esphome.automation import automation_is_synchronous
from esphome.const import CONF_ELSE, CONF_THEN


def test_automation_is_synchronous_empty_list() -> None:
    """Test that empty action list is considered synchronous."""
    assert automation_is_synchronous([]) is True


def test_automation_is_synchronous_single_sync_action() -> None:
    """Test that a single synchronous action returns True."""
    actions = [{"logger.log": {"message": "Hello"}}]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_multiple_sync_actions() -> None:
    """Test that multiple synchronous actions return True."""
    actions = [
        {"logger.log": {"message": "First"}},
        {"lambda": "id(my_sensor).publish_state(42);"},
        {"logger.log": {"message": "Second"}},
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_delay_action() -> None:
    """Test that delay action makes automation async."""
    actions = [{"delay": "1s"}]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_delay_in_middle() -> None:
    """Test that delay in the middle of actions makes automation async."""
    actions = [
        {"logger.log": {"message": "Before"}},
        {"delay": "100ms"},
        {"logger.log": {"message": "After"}},
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_wait_until_action() -> None:
    """Test that wait_until action makes automation async."""
    actions = [{"wait_until": {"condition": {"lambda": "return true;"}}}]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_wait_until_with_timeout() -> None:
    """Test that wait_until with timeout makes automation async."""
    actions = [
        {
            "wait_until": {
                "condition": {"lambda": "return true;"},
                "timeout": "5s",
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


# Nested if tests
def test_automation_is_synchronous_if_sync_then() -> None:
    """Test that if with sync then block is synchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_if_async_then() -> None:
    """Test that if with async then block is asynchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"delay": "1s"}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_if_sync_else() -> None:
    """Test that if with sync else block is synchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
                CONF_ELSE: [{"logger.log": {"message": "Else"}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_if_async_else() -> None:
    """Test that if with async else block is asynchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
                CONF_ELSE: [{"delay": "1s"}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_if_sync_then_async_else() -> None:
    """Test that if with sync then but async else is asynchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
                CONF_ELSE: [{"wait_until": {"condition": {"lambda": "return true;"}}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_if_async_then_sync_else() -> None:
    """Test that if with async then but sync else is asynchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"delay": "500ms"}],
                CONF_ELSE: [{"logger.log": {"message": "Else"}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


# Nested while tests
def test_automation_is_synchronous_while_sync_body() -> None:
    """Test that while with sync body is synchronous."""
    actions = [
        {
            "while": {
                "condition": {"lambda": "return false;"},
                CONF_THEN: [{"logger.log": {"message": "Loop"}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_while_async_body() -> None:
    """Test that while with async body is asynchronous."""
    actions = [
        {
            "while": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"delay": "100ms"}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


# Nested repeat tests
def test_automation_is_synchronous_repeat_sync_body() -> None:
    """Test that repeat with sync body is synchronous."""
    actions = [
        {
            "repeat": {
                "count": 5,
                CONF_THEN: [{"logger.log": {"message": "Iteration"}}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_repeat_async_body() -> None:
    """Test that repeat with async body is asynchronous."""
    actions = [
        {
            "repeat": {
                "count": 3,
                CONF_THEN: [{"delay": "50ms"}],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


# Deeply nested tests
def test_automation_is_synchronous_deeply_nested_sync() -> None:
    """Test that deeply nested sync actions are synchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [
                    {
                        "while": {
                            "condition": {"lambda": "return false;"},
                            CONF_THEN: [
                                {
                                    "repeat": {
                                        "count": 3,
                                        CONF_THEN: [
                                            {"logger.log": {"message": "Deep"}}
                                        ],
                                    }
                                }
                            ],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_deeply_nested_async_in_repeat() -> None:
    """Test that deeply nested delay in repeat makes automation async."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [
                    {
                        "while": {
                            "condition": {"lambda": "return false;"},
                            CONF_THEN: [
                                {
                                    "repeat": {
                                        "count": 3,
                                        CONF_THEN: [{"delay": "10ms"}],
                                    }
                                }
                            ],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_deeply_nested_async_in_while() -> None:
    """Test that deeply nested wait_until in while makes automation async."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [
                    {
                        "while": {
                            "condition": {"lambda": "return true;"},
                            CONF_THEN: [
                                {
                                    "wait_until": {
                                        "condition": {"lambda": "return true;"}
                                    }
                                }
                            ],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_deeply_nested_async_in_else() -> None:
    """Test that deeply nested delay in else branch makes automation async."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
                CONF_ELSE: [
                    {
                        "if": {
                            "condition": {"lambda": "return false;"},
                            CONF_THEN: [{"logger.log": {"message": "Inner then"}}],
                            CONF_ELSE: [{"delay": "1s"}],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_multiple_nested_ifs_all_sync() -> None:
    """Test multiple nested if statements that are all synchronous."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [
                    {
                        "if": {
                            "condition": {"lambda": "return false;"},
                            CONF_THEN: [{"logger.log": {"message": "A"}}],
                            CONF_ELSE: [{"logger.log": {"message": "B"}}],
                        }
                    }
                ],
                CONF_ELSE: [
                    {
                        "if": {
                            "condition": {"lambda": "return true;"},
                            CONF_THEN: [{"logger.log": {"message": "C"}}],
                            CONF_ELSE: [{"logger.log": {"message": "D"}}],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_mixed_sync_async_sequence() -> None:
    """Test sequence with sync action followed by async action."""
    actions = [
        {"logger.log": {"message": "First"}},
        {"lambda": "id(sensor).publish_state(1);"},
        {"delay": "1s"},  # This makes it async
        {"logger.log": {"message": "Last"}},
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_unknown_action_treated_as_sync() -> None:
    """Test that unknown actions (not in registry) are treated as sync by default."""
    # This simulates an external component's action that isn't registered
    actions = [{"my_custom_component.do_something": {"param": "value"}}]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_empty_if_branches() -> None:
    """Test if with missing then/else branches."""
    # Just condition, no then or else
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
            }
        }
    ]
    assert automation_is_synchronous(actions) is True


def test_automation_is_synchronous_repeat_inside_if_else() -> None:
    """Test repeat with delay inside if else branch."""
    actions = [
        {
            "if": {
                "condition": {"lambda": "return true;"},
                CONF_THEN: [{"logger.log": {"message": "Then"}}],
                CONF_ELSE: [
                    {
                        "repeat": {
                            "count": 5,
                            CONF_THEN: [
                                {"logger.log": {"message": "Repeat"}},
                                {"delay": "100ms"},
                            ],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False


def test_automation_is_synchronous_while_inside_repeat() -> None:
    """Test while with wait_until inside repeat."""
    actions = [
        {
            "repeat": {
                "count": 3,
                CONF_THEN: [
                    {
                        "while": {
                            "condition": {"lambda": "return true;"},
                            CONF_THEN: [
                                {
                                    "wait_until": {
                                        "condition": {"lambda": "return false;"}
                                    }
                                }
                            ],
                        }
                    }
                ],
            }
        }
    ]
    assert automation_is_synchronous(actions) is False
