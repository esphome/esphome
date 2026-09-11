import os

Import("env")  # noqa: F821

# Remove custom_sdkconfig from the board config as it causes
# pioarduino to enable some strange hybrid build mode that breaks IDF
board = env.BoardConfig()
if "espidf.custom_sdkconfig" in board:
    del board._manifest["espidf"]["custom_sdkconfig"]
    if not board._manifest["espidf"]:
        del board._manifest["espidf"]

# Referenced by rules in esphome/idf_component.yml; an unset env var is a
# fatal error there. Always 0: in PlatformIO builds arduino is not a managed
# IDF component.
os.environ.setdefault("ESPHOME_ARDUINO_COMPONENT", "0")
