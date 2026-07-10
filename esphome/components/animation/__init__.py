# ---------------------------------------------------------------------------
# Legacy top-level `animation:` deprecation shim -- REMOVE this whole file after
# 2027.1.0.
#
# Animations are now a platform of the `image:` component (`platform:
# animation`); the real schema, actions and codegen live in `image.py`. This
# module only keeps the deprecated top-level `animation:` key working during the
# deprecation window: it reuses that schema/codegen and adds a one-shot
# deprecation warning (with a pasteable migrated `image:` block) at validation
# time. Deleting this file drops the top-level form entirely.
# ---------------------------------------------------------------------------

import esphome.components.image as espImage
import esphome.config_validation as cv

from .image import ANIMATION_CONFIG_SCHEMA, setup_animation

AUTO_LOAD = ["image", "file"]
CODEOWNERS = ["@syndlex"]
DEPENDENCIES = ["display"]
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

DOMAIN = "animation"

LEGACY_REMOVAL_VERSION = "2027.1.0"

_capture_legacy_entry, _warn_legacy_animation = (
    espImage.legacy_platform_migration_warning(DOMAIN, DOMAIN, LEGACY_REMOVAL_VERSION)
)

CONFIG_SCHEMA = cv.All(_capture_legacy_entry, ANIMATION_CONFIG_SCHEMA)

FINAL_VALIDATE_SCHEMA = _warn_legacy_animation

to_code = setup_animation
