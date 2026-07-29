# ---------------------------------------------------------------------------
# Legacy top-level `online_image:` deprecation shim -- REMOVE this whole file
# after 2027.1.0.
#
# Online images are now a platform of the `image:` component (`platform:
# online_image`); the real schema, actions and codegen live in `image.py`. This
# module only keeps the deprecated top-level `online_image:` key working during
# the deprecation window: it reuses that schema/codegen and adds a one-shot
# deprecation warning (with a pasteable migrated `image:` block) at validation
# time. Deleting this file drops the top-level form entirely.
# ---------------------------------------------------------------------------

import esphome.components.image as espImage
import esphome.config_validation as cv

from .image import ONLINE_IMAGE_CONFIG_SCHEMA, setup_online_image

AUTO_LOAD = ["image", "runtime_image"]
DEPENDENCIES = ["display", "http_request"]
CODEOWNERS = ["@guillempages", "@clydebarrow"]
MULTI_CONF = True

DOMAIN = "online_image"

LEGACY_REMOVAL_VERSION = "2027.1.0"

_capture_legacy_entry, _warn_legacy_online_image = (
    espImage.legacy_platform_migration_warning(DOMAIN, DOMAIN, LEGACY_REMOVAL_VERSION)
)

CONFIG_SCHEMA = cv.All(_capture_legacy_entry, ONLINE_IMAGE_CONFIG_SCHEMA)

FINAL_VALIDATE_SCHEMA = _warn_legacy_online_image

to_code = setup_online_image
