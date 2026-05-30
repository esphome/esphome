"""clang-tidy idedata from an ESP-IDF native-toolchain build.

Thin wrapper that delegates to ``esphome.espidf.idedata`` so the
compile_commands.json -> idedata transform lives in one place (the esphome
package, which also uses it during ``esphome compile`` for IDE integration).
"""

from esphome.espidf.idedata import idedata_from_compile_commands

__all__ = ["idedata_from_compile_commands"]
