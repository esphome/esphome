Import("env")  # noqa: F821

# Remove custom_sdkconfig from the board config as it causes
# pioarduino to enable some strange hybrid build mode that breaks IDF
board = env.BoardConfig()
if "espidf.custom_sdkconfig" in board:
    del board._manifest["espidf"]["custom_sdkconfig"]
    if not board._manifest["espidf"]:
        del board._manifest["espidf"]
