"""Back-compat shim for ``friendly_name_slugify``.

The function moved to :mod:`esphome.helpers` so it survives the legacy
dashboard's eventual removal — see the
``esphome.helpers.friendly_name_slugify`` docstring. This module
re-exports the name so existing
``from esphome.dashboard.util.text import friendly_name_slugify``
imports keep working while downstream consumers migrate.
"""

from __future__ import annotations

from esphome.helpers import friendly_name_slugify

__all__ = ["friendly_name_slugify"]
