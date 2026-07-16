from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
import logging
from pathlib import Path

from PIL import Image, UnidentifiedImageError

import esphome.codegen as cg
from esphome.components.const import CONF_BYTE_ORDER, KEY_METADATA
import esphome.config_validation as cv
from esphome.const import CONF_DEFAULTS, CONF_FILE, CONF_ID, CONF_PLATFORM, CONF_TYPE
from esphome.core import CORE
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DOMAIN = "image"
DEPENDENCIES = ["display"]
IS_PLATFORM_COMPONENT = True

# Name of the built-in static-image platform (local file / web / MDI sources).
PLATFORM_FILE = "file"

image_ns = cg.esphome_ns.namespace("image")

ImageType = image_ns.enum("ImageType")


@dataclass(frozen=True)
class ImageMetaData:
    width: int
    height: int
    image_type: str
    transparency: str


CONF_OPAQUE = "opaque"
CONF_CHROMA_KEY = "chroma_key"
CONF_ALPHA_CHANNEL = "alpha_channel"
CONF_INVERT_ALPHA = "invert_alpha"
CONF_IMAGES = "images"

TRANSPARENCY_TYPES = (
    CONF_OPAQUE,
    CONF_CHROMA_KEY,
    CONF_ALPHA_CHANNEL,
)


def get_image_type_enum(type):
    return getattr(ImageType, f"IMAGE_TYPE_{type.upper()}")


def get_transparency_enum(transparency):
    return getattr(TransparencyType, f"TRANSPARENCY_{transparency.upper()}")


class ImageEncoder:
    """
    Superclass of image type encoders
    """

    # Control which transparency options are available for a given type
    allow_config = {CONF_ALPHA_CHANNEL, CONF_CHROMA_KEY, CONF_OPAQUE}

    # All imageencoder types are valid
    @staticmethod
    def validate(value):
        return value

    def __init__(self, width, height, transparency, dither, invert_alpha):
        """
        :param width:  The image width in pixels (or bytes)
        :param height:  The image height in pixels
        :param transparency: Transparency type
        :param dither: Dither method
        :param invert_alpha: True if the alpha channel should be inverted; for monochrome formats inverts the colours.
        """
        self.transparency = transparency
        self.width = width
        self.height = height
        self.data = [0] * width * height
        self.dither = dither
        self.index = 0
        self.invert_alpha = invert_alpha
        self.path = ""
        self.big_endian = False

    def convert(self, image, path):
        """
        Convert the image format
        :param image:  Input image
        :param path:  Path to the image file
        :return: converted image
        """
        return image

    def encode(self, pixel):
        """
        Encode a single pixel
        """

    def end_row(self):
        """
        Marks the end of a pixel row
        :return:
        """

    def end_image(self):
        """
        Called at the end of the image.
        :return:
        """

    def set_big_endian(self, big_endian: bool) -> None:
        self.big_endian = big_endian

    @classmethod
    def is_endian(cls) -> bool:
        """
        Check if the image encoder supports endianness configuration
        """
        return False


def is_alpha_only(image: Image):
    """
    Check if an image (assumed to be RGBA) is only alpha
    """
    # Any alpha data?
    if image.split()[-1].getextrema()[0] == 0xFF:
        return False
    return all(b.getextrema()[1] == 0 for b in image.split()[:-1])


class ImageBinary(ImageEncoder):
    allow_config = {CONF_OPAQUE, CONF_INVERT_ALPHA, CONF_CHROMA_KEY}

    def __init__(self, width, height, transparency, dither, invert_alpha):
        self.width8 = (width + 7) // 8
        super().__init__(self.width8, height, transparency, dither, invert_alpha)
        self.bitno = 0

    def convert(self, image, path):
        if is_alpha_only(image):
            image = image.split()[-1]
        return image.convert("1", dither=self.dither)

    def encode(self, pixel):
        if self.invert_alpha:
            pixel = not pixel
        if pixel:
            self.data[self.index] |= 0x80 >> (self.bitno % 8)
        self.bitno += 1
        if self.bitno == 8:
            self.bitno = 0
            self.index += 1

    def end_row(self):
        """
        Pad rows to a byte boundary
        """
        if self.bitno != 0:
            self.bitno = 0
            self.index += 1


class ImageGrayscale(ImageEncoder):
    allow_config = {CONF_ALPHA_CHANNEL, CONF_CHROMA_KEY, CONF_INVERT_ALPHA, CONF_OPAQUE}

    def convert(self, image, path):
        if is_alpha_only(image):
            if self.transparency != CONF_ALPHA_CHANNEL:
                _LOGGER.warning(
                    "Grayscale image %s is alpha only, but transparency is set to %s",
                    path,
                    self.transparency,
                )
                self.transparency = CONF_ALPHA_CHANNEL
            image = image.split()[-1]
        return image.convert("LA")

    def encode(self, pixel):
        b, a = pixel
        if self.transparency == CONF_CHROMA_KEY:
            if b == 1:
                b = 0
            if a != 0xFF:
                b = 1
        if self.invert_alpha:
            b ^= 0xFF
        if self.transparency == CONF_ALPHA_CHANNEL and a != 0xFF:
            b = a
        self.data[self.index] = b
        self.index += 1


class ImageRGB565(ImageEncoder):
    def __init__(self, width, height, transparency, dither, invert_alpha):
        super().__init__(
            width * 2,
            height,
            transparency,
            dither,
            invert_alpha,
        )
        self.alpha = [0] * width * height

    @classmethod
    def is_endian(cls) -> bool:
        """
        Check if the image encoder supports endianness configuration
        """
        return True

    def convert(self, image, path):
        return image.convert("RGBA")

    def encode(self, pixel):
        r, g, b, a = pixel
        r = r >> 3
        g = g >> 2
        b = b >> 3
        if self.invert_alpha:
            a ^= 0xFF
        self.alpha[self.index // 2] = a
        if self.transparency == CONF_CHROMA_KEY:
            if r == 0 and g == 1 and b == 0:
                g = 0
            elif a < 128:
                r = 0
                g = 1
                b = 0
        rgb = (r << 11) | (g << 5) | b
        if self.big_endian:
            self.data[self.index] = rgb >> 8
            self.index += 1
            self.data[self.index] = rgb & 0xFF
            self.index += 1
        else:
            self.data[self.index] = rgb & 0xFF
            self.index += 1
            self.data[self.index] = rgb >> 8
            self.index += 1

    def end_image(self):
        if self.transparency == CONF_ALPHA_CHANNEL:
            self.data.extend(self.alpha)


class ImageRGB(ImageEncoder):
    def __init__(self, width, height, transparency, dither, invert_alpha):
        stride = 4 if transparency == CONF_ALPHA_CHANNEL else 3
        super().__init__(
            width * stride,
            height,
            transparency,
            dither,
            invert_alpha,
        )

    def convert(self, image, path):
        return image.convert("RGBA")

    def encode(self, pixel):
        r, g, b, a = pixel
        if self.transparency == CONF_CHROMA_KEY:
            if r == 0 and g == 1 and b == 0:
                g = 0
            elif a < 128:
                r = 0
                g = 1
                b = 0
        self.data[self.index] = b
        self.index += 1
        self.data[self.index] = g
        self.index += 1
        self.data[self.index] = r
        self.index += 1
        if self.transparency == CONF_ALPHA_CHANNEL:
            if self.invert_alpha:
                a ^= 0xFF
            self.data[self.index] = a
            self.index += 1


class ReplaceWith:
    """
    Placeholder class to provide feedback on deprecated features
    """

    allow_config = {CONF_ALPHA_CHANNEL, CONF_CHROMA_KEY, CONF_OPAQUE}

    def __init__(self, replace_with):
        self.replace_with = replace_with

    def validate(self, value):
        raise cv.Invalid(
            f"Image type {value} is removed; replace with {self.replace_with}"
        )


IMAGE_TYPE = {
    "BINARY": ImageBinary,
    "GRAYSCALE": ImageGrayscale,
    "RGB565": ImageRGB565,
    "RGB": ImageRGB,
    "TRANSPARENT_BINARY": ReplaceWith("'type: BINARY' and 'transparency: chroma_key'"),
    "RGB24": ReplaceWith("'type: RGB'"),
    "RGBA": ReplaceWith("'type: RGB' and 'transparency: alpha_channel'"),
}

TransparencyType = image_ns.enum("TransparencyType")

CONF_TRANSPARENCY = "transparency"

Image_ = image_ns.class_("Image")

INSTANCE_TYPE = Image_


def is_svg_file(file):
    if not file:
        return False
    with Path(file).open("rb") as f:
        return "<svg" in str(f.read(1024))


def validate_transparency(choices=TRANSPARENCY_TYPES):
    def validate(value):
        if isinstance(value, bool):
            value = str(value)
        return cv.one_of(*choices, lower=True)(value)

    return validate


def validate_type(image_types):
    def validate(value):
        value = cv.one_of(*image_types, upper=True)(value)
        return IMAGE_TYPE[value].validate(value)

    return validate


def validate_settings(value, path=()):
    """
    Validate the settings for a single image configuration.
    """
    conf_type = value[CONF_TYPE]
    type_class = IMAGE_TYPE[conf_type]

    transparency = value.get(CONF_TRANSPARENCY, CONF_OPAQUE).lower()
    if transparency not in type_class.allow_config:
        raise cv.Invalid(
            f"Image format '{conf_type}' cannot have transparency: {transparency}"
        )
    invert_alpha = value.get(CONF_INVERT_ALPHA, False)
    if (
        invert_alpha
        and transparency != CONF_ALPHA_CHANNEL
        and CONF_INVERT_ALPHA not in type_class.allow_config
    ):
        raise cv.Invalid("No alpha channel to invert")
    if value.get(CONF_BYTE_ORDER) is not None and not type_class.is_endian():
        raise cv.Invalid(
            f"Image format '{conf_type}' does not support byte order configuration",
            path=path,
        )
    if file := value.get(CONF_FILE):
        file = Path(file)
        if not is_svg_file(file):
            try:
                Image.open(file)
            except UnidentifiedImageError as exc:
                raise cv.Invalid(
                    f"File can't be opened as image: {file.absolute()}", path=path
                ) from exc
    return value


def add_metadata(id: str, width: int, height: int, image_type: str, transparency):
    all_metadata = CORE.data.setdefault(DOMAIN, {}).setdefault(KEY_METADATA, {})
    all_metadata[str(id)] = ImageMetaData(
        width=width, height=height, image_type=image_type, transparency=transparency
    )


async def to_code(config: ConfigType) -> None:
    # Base platform-component codegen: each entry is generated by its platform's
    # own ``to_code``; here we only need the feature define to be present.
    cg.add_define("USE_IMAGE")


def get_all_image_metadata() -> dict[str, ImageMetaData]:
    """Get all image metadata."""
    return CORE.data.get(DOMAIN, {}).get(KEY_METADATA, {})


def get_image_metadata(image_id: str) -> ImageMetaData | None:
    """Get image metadata by ID for use by other components."""
    return get_all_image_metadata().get(image_id)


# ---------------------------------------------------------------------------
# Legacy top-level component -> `image:` platform deprecation helpers
# -- REMOVE after 2027.1.0 together with the `animation:`/`online_image:` shims.
#
# `animation:` and `online_image:` used to be standalone top-level components and
# are now platforms of `image:`. Their deprecated top-level shims use this helper
# to (1) record each raw entry as it is validated and (2) print a single,
# pasteable migrated `image:` block once every entry has been seen. The block is
# emitted from FINAL_VALIDATE_SCHEMA, which always runs after every per-entry
# CONFIG_SCHEMA step, so all entries are captured before it fires.
# ---------------------------------------------------------------------------


def legacy_platform_migration_warning(
    domain: str, platform: str, removal_version: str
) -> tuple[
    Callable[[ConfigType], ConfigType],
    Callable[[ConfigType], ConfigType],
]:
    """Build the per-entry capture and one-shot warning validators for a
    deprecated top-level component that is now an ``image:`` platform.

    Returns ``(capture, finalize)``:
    * ``capture`` is a ``CONFIG_SCHEMA`` validator placed *before* the real
      schema so it sees the raw user entry; it records a copy of each entry.
    * ``finalize`` is a ``FINAL_VALIDATE_SCHEMA`` validator that warns exactly
      once with the migrated, pasteable ``image:`` block.
    """
    entries_key = "legacy_entries"
    shown_key = "legacy_warning_shown"

    def capture(config: ConfigType) -> ConfigType:
        data = CORE.data.setdefault(domain, {})
        data.setdefault(entries_key, []).append(dict(config))
        return config

    def finalize(config: ConfigType) -> ConfigType:
        data = CORE.data.setdefault(domain, {})
        if not data.get(shown_key):
            data[shown_key] = True

            from esphome import yaml_util

            migrated = [
                {CONF_PLATFORM: platform, **entry}
                for entry in data.get(entries_key, [])
            ]
            _LOGGER.warning(
                "The top-level '%s:' configuration is deprecated and will be "
                "removed in ESPHome %s. '%s' is now a platform of the 'image' "
                "component. Replace your '%s:' block with:\n\n%s",
                domain,
                removal_version,
                domain,
                domain,
                yaml_util.dump({DOMAIN: migrated}),
            )
        return config

    return capture, finalize


# ---------------------------------------------------------------------------
# Legacy `image:` config migration -- REMOVE after 2027.1.0
#
# Before `image` became a platform component, its top-level config was either a
# bare list of image dicts, a single image dict, or a dict with `defaults:`,
# `images:` and per-type group keys. This block transparently rewrites those
# forms into the new ``platform: file`` list and prints the migrated YAML.
# It is intentionally self-contained so it can be deleted in one piece together
# with the ``LEGACY_CONFIG_MIGRATE`` assignment below.
# ---------------------------------------------------------------------------

LEGACY_REMOVAL_VERSION = "2027.1.0"


def _is_new_image_format(config: object) -> bool:
    """True when the config is already the new ``platform:``-tagged list."""
    return isinstance(config, list) and all(
        isinstance(entry, dict) and CONF_PLATFORM in entry for entry in config
    )


def _is_legacy_image_format(config: object) -> bool:
    """True when ``config`` matches a shape the pre-platform schema accepted.

    Only these shapes are migrated. Anything else -- a list containing a
    non-dict (or already platform-tagged) entry, or a dict with no recognised
    image keys -- is left untouched so the platform validation surfaces a
    proper error instead of the migration silently dropping the input.
    """
    if isinstance(config, list):
        # A bare list of (not-yet-platform-tagged) image dicts.
        return bool(config) and all(
            isinstance(entry, dict) and CONF_PLATFORM not in entry for entry in config
        )
    if not isinstance(config, dict):
        return False
    # A single image dict, or the grouped `defaults:`/`images:`/type-key form.
    return (
        CONF_ID in config
        or CONF_FILE in config
        or any(
            key in (CONF_DEFAULTS, CONF_IMAGES) or key.upper() in IMAGE_TYPE
            for key in config
        )
    )


def _flatten_legacy_image_config(config: object) -> list[dict]:
    """Structurally flatten a legacy ``image:`` config into image dicts.

    No validation or file IO is performed -- the ``file`` platform schema
    validates the resulting entries. Unrecognised shapes yield no entries so the
    normal platform validation surfaces the error.
    """
    if isinstance(config, list):
        return [dict(entry) for entry in config if isinstance(entry, dict)]
    if not isinstance(config, dict):
        return []
    if CONF_ID in config or CONF_FILE in config:
        return [dict(config)]

    defaults = config.get(CONF_DEFAULTS) or {}
    result: list[dict] = []

    def _add(entry: dict, extra: dict) -> None:
        merged = {**defaults, **extra, **entry}
        # The legacy `defaults:`/type-grouped forms only applied `byte_order` to
        # types that support it. Replicate that so an endian default merged into
        # e.g. a binary image stays valid.
        type_class = IMAGE_TYPE.get(str(merged.get(CONF_TYPE, "")).upper())
        if (
            CONF_BYTE_ORDER in merged
            and isinstance(type_class, type)
            and issubclass(type_class, ImageEncoder)
            and not type_class.is_endian()
        ):
            del merged[CONF_BYTE_ORDER]
        result.append(merged)

    def _add_entries(entries: object, extra: dict) -> None:
        # `entries` may be a single image dict or a list of them; non-dict
        # members are silently skipped, mirroring the old `ensure_list` leniency.
        for entry in [entries] if isinstance(entries, dict) else entries:
            if isinstance(entry, dict):
                _add(entry, extra)

    _add_entries(config.get(CONF_IMAGES, []), {})

    for key, value in config.items():
        if key in (CONF_DEFAULTS, CONF_IMAGES) or key.upper() not in IMAGE_TYPE:
            continue
        type_extra = {CONF_TYPE: key}
        if isinstance(value, dict) and (
            transparency_keys := [k for k in value if k in TRANSPARENCY_TYPES]
        ):
            for trans in transparency_keys:
                _add_entries(value[trans], {**type_extra, CONF_TRANSPARENCY: trans})
        elif isinstance(value, (list, dict)):
            _add_entries(value, type_extra)
    return result


def _migrate_legacy_image_config(config: object) -> list[dict] | None:
    """Rewrite a legacy ``image:`` config into the ``platform: file`` list.

    Returns None for the already-migrated platform form and for any shape the
    pre-platform schema never accepted, so normal platform validation can
    surface a proper error instead of the migration silently discarding input.
    """
    if _is_new_image_format(config) or not _is_legacy_image_format(config):
        return None
    migrated = [
        {CONF_PLATFORM: PLATFORM_FILE, **entry}
        for entry in _flatten_legacy_image_config(config)
    ]

    from esphome import yaml_util

    _LOGGER.warning(
        "The 'image:' configuration format is deprecated and will be removed in "
        "ESPHome %s. Images are now platforms of the 'image' component. Replace "
        "your 'image:' block with:\n\n%s",
        LEGACY_REMOVAL_VERSION,
        yaml_util.dump({DOMAIN: migrated}),
    )
    return migrated


LEGACY_CONFIG_MIGRATE = _migrate_legacy_image_config

# --------------------------- end legacy migration --------------------------
