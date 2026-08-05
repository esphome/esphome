"""Backward-compatibility shim; the log client lives in esphome.api_client.

Importing this module executes the whole api component package, which pulls
in the validation stack. CLI code paths should import esphome.api_client
directly so the logs fast path stays light.
"""

from esphome.api_client import async_run_logs, run_logs

__all__ = ["async_run_logs", "run_logs"]
