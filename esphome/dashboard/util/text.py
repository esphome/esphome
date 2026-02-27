from __future__ import annotations

from esphome.helpers import slugify


def friendly_name_slugify(value: str) -> str:
    """Convert a friendly name to a slug with dashes instead of underscores."""
    # First use the standard slugify, then convert underscores to dashes
    return slugify(value).replace("_", "-")
