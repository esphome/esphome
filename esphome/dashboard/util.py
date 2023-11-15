import hashlib
import unicodedata
from collections.abc import Iterable
from functools import partial
from itertools import islice
from typing import Any

from esphome.const import ALLOWED_NAME_CHARS


def password_hash(password: str) -> bytes:
    """Create a hash of a password to transform it to a fixed-length digest.

    Note this is not meant for secure storage, but for securely comparing passwords.
    """
    return hashlib.sha256(password.encode()).digest()


def strip_accents(value):
    return "".join(
        c
        for c in unicodedata.normalize("NFD", str(value))
        if unicodedata.category(c) != "Mn"
    )


def friendly_name_slugify(value):
    value = (
        strip_accents(value)
        .lower()
        .replace(" ", "-")
        .replace("_", "-")
        .replace("--", "-")
        .strip("-")
    )
    return "".join(c for c in value if c in ALLOWED_NAME_CHARS)


def take(take_num: int, iterable: Iterable) -> list[Any]:
    """Return first n items of the iterable as a list.

    From itertools recipes
    """
    return list(islice(iterable, take_num))


def chunked(iterable: Iterable, chunked_num: int) -> Iterable[Any]:
    """Break *iterable* into lists of length *n*.

    From more-itertools
    """
    return iter(partial(take, chunked_num, iter(iterable)), [])
