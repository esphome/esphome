# Importing `esphome.loader` here installs the component-alias
# ``sys.meta_path`` finder before any submodule lookup runs. Without this,
# `from esphome.components import <legacy_alias>` from a fresh interpreter
# can race the finder install and raise ImportError, since the legacy
# alias dir no longer exists on disk.
from esphome import loader as _loader  # noqa: F401
