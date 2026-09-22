"""Configuration keys used by the `mipi_csi` component.

These live apart from `__init__.py` so that `validate.py` can use them without importing the
component back into itself.
"""

CONF_EXTERNAL_CLOCK = "external_clock"
CONF_FRAME_BUFFER_COUNT = "frame_buffer_count"
CONF_FRAMERATE = "framerate"
CONF_HORIZONTAL_FLIP = "horizontal_flip"
CONF_IDLE_FRAMERATE = "idle_framerate"
CONF_INIT_LDO = "init_ldo"
CONF_JPEG_QUALITY = "jpeg_quality"
CONF_ON_IMAGE = "on_image"
CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_PIXEL_FORMAT = "pixel_format"
CONF_POWER_PIN = "power_pin"
CONF_SCCB_FREQUENCY = "sccb_frequency"
CONF_VERTICAL_FLIP = "vertical_flip"
