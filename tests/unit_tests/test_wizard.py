""" Tests for the wizard.py file """

import esphome.wizard as wz


def test_sanitize_quotes_replaces_with_escaped_char():
    """
    The sanitize_quotes function should replace double quotes with their escaped equivalents
    """
    # Given
    input_str = "\"key\": \"value\""

    # When
    output_str = wz.sanitize_double_quotes(input_str)

    # Then
    assert output_str == "\\\"key\\\": \\\"value\\\""
