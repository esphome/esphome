"""Shared tail for the lazy-import fixture scripts."""

import sys


def print_leaked_modules() -> None:
    """Report argv-listed heavy modules (plus any component package) loaded.

    Any component package counts as a leak, not just the ones on the
    watch list: executing one drags in codegen/validation machinery by
    design.
    """
    leaked = [module for module in sys.argv[1:] if module in sys.modules]
    leaked += [
        module
        for module in sys.modules
        if module.startswith("esphome.components.") and module not in leaked
    ]
    print(",".join(leaked))
