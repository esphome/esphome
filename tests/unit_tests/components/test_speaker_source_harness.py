"""Guard extraction against harmless formatting, comment and literal changes."""

import pytest
from speaker_source_test_helpers import method


@pytest.mark.parametrize(
    "statement",
    [
        "// unmatched {\n",
        "/* unmatched } */",
        'const char *s = "{\\"}";',
        "char c = '}';",
        'const char *s = R"tag({"})tag";',
        "if (true) { return; }",
    ],
)
def test_handler_literals(statement: str) -> None:
    handler = f"void\nPlayer::stop \n() {{\n{statement}\n}}"
    assert method(handler + "\nvoid other() {}", "Player::stop") == handler


def test_missing_handler() -> None:
    with pytest.raises(AssertionError, match="Missing"):
        method("", "Player::stop")


def test_unterminated_handler() -> None:
    with pytest.raises(AssertionError, match="Unterminated"):
        method("void Player::stop() {", "Player::stop")
