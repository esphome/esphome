import pytest

from esphome.log import AnsiFore, AnsiStyle, color


def test_color_keep_returns_unchanged_message() -> None:
    """Test that AnsiFore.KEEP returns the message unchanged."""
    msg = "test message"
    result = color(AnsiFore.KEEP, msg)
    assert result == msg


def test_color_keep_ignores_reset_parameter() -> None:
    """Test that reset parameter is ignored when using AnsiFore.KEEP."""
    msg = "test message"
    result_with_reset = color(AnsiFore.KEEP, msg, reset=True)
    result_without_reset = color(AnsiFore.KEEP, msg, reset=False)
    assert result_with_reset == msg
    assert result_without_reset == msg


def test_color_applies_color_code() -> None:
    """Test that color codes are properly applied to messages."""
    msg = "test message"
    result = color(AnsiFore.RED, msg, reset=False)
    assert result == f"{AnsiFore.RED.value}{msg}"


def test_color_applies_reset_when_requested() -> None:
    """Test that RESET_ALL is added when reset=True."""
    msg = "test message"
    result = color(AnsiFore.GREEN, msg, reset=True)
    expected = f"{AnsiFore.GREEN.value}{msg}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


def test_color_no_reset_when_not_requested() -> None:
    """Test that RESET_ALL is not added when reset=False."""
    msg = "test message"
    result = color(AnsiFore.BLUE, msg, reset=False)
    expected = f"{AnsiFore.BLUE.value}{msg}"
    assert result == expected


def test_color_with_empty_message() -> None:
    """Test color function with empty message."""
    result = color(AnsiFore.YELLOW, "", reset=True)
    expected = f"{AnsiFore.YELLOW.value}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


@pytest.mark.parametrize(
    "col",
    [
        AnsiFore.BLACK,
        AnsiFore.RED,
        AnsiFore.GREEN,
        AnsiFore.YELLOW,
        AnsiFore.BLUE,
        AnsiFore.MAGENTA,
        AnsiFore.CYAN,
        AnsiFore.WHITE,
        AnsiFore.RESET,
    ],
)
def test_all_ansi_colors(col: AnsiFore) -> None:
    """Test that all AnsiFore colors work correctly."""
    msg = "test"
    result = color(col, msg, reset=True)
    expected = f"{col.value}{msg}{AnsiStyle.RESET_ALL.value}"
    assert result == expected


def test_ansi_fore_keep_is_enum_member() -> None:
    """Ensure AnsiFore.KEEP is an Enum member and evaluates to truthy."""
    assert isinstance(AnsiFore.KEEP, AnsiFore)
    # Enum members are truthy, even with empty string values
    assert bool(AnsiFore.KEEP) is True
    # But the value itself is still an empty string
    assert AnsiFore.KEEP.value == ""
