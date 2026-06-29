"""ESPHome.

ESPHome validates configuration with probatio. External (third-party) components
still ``import voluptuous`` directly, so probatio's compatibility shim is installed
here, at package import, before any submodule or component imports voluptuous. This
aliases ``voluptuous`` (and the submodules dependencies reach into) to probatio in
``sys.modules`` for the lifetime of the process.
"""

from probatio.compat import install_as_voluptuous

install_as_voluptuous()
