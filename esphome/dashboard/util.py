import hashlib
import unicodedata

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
