"""Byte-size parsing for configuration values such as ``tensor_arena_size``."""

import re

import esphome.config_validation as cv


def datasize_to_bytes(value: object) -> int:
    try:
        text = re.sub(r"\bbytes?\b", "B", str(value).strip(), flags=re.IGNORECASE)
        text = text.upper()
        for suffix, factor in (
            ("GB", 1024**3),
            ("MB", 1024**2),
            ("KB", 1024),
            # Single-letter forms such as "64K" are common in reports.
            ("G", 1024**3),
            ("M", 1024**2),
            ("K", 1024),
            ("B", 1),
        ):
            if text.endswith(suffix):
                return int(float(text[: -len(suffix)].strip()) * factor)
        return int(float(text))
    except ValueError as e:
        raise cv.Invalid(f"Invalid data size: {value}") from e
