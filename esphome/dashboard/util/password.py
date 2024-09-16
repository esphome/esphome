from __future__ import annotations

import hashlib


def password_hash(password: str) -> bytes:
    """Create a hash of a password to transform it to a fixed-length digest.

    Note this is not meant for secure storage, but for securely comparing passwords.
    """
    return hashlib.sha256(password.encode()).digest()
