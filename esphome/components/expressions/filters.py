from typing import Union

ConvertibleIntoBool = Union[str, int, float, bool]


def filter_bool(value: ConvertibleIntoBool) -> bool:
    """
    Converts input into boolean. Expects string representing boolean or number.
    In case of string the true will be generated for strings "true", "yes", "1"
    and false for any other string.
    For number input true will be generated for any value but 0.
    For boolean input the input will be returned back as it is
    :param value: value to be converted
    :raises: ValueError in case of any other input type given
    :return: boolean representation of the input
    """
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in ["true", "yes", "1"]
    if isinstance(value, (int, float)):
        return value != 0
    raise ValueError("Value {} can't be converted into boolean".format(value))


def filter_not(value: ConvertibleIntoBool) -> bool:
    """
    Returns the value opposite to the input.
    If input is not of the boolean type it will try to convert it first (see filter_bool)
    :param value:
    :raises: ValueError if input can't be converted into boolean
    :return: value opposite to the given input
    """
    return not filter_bool(value)
