"""Shared setup for the LVGL component tests."""

# Trigger tests use the logger.log action, which is only registered once the logger
# component is imported. Without this, they pass or fail depending on test order.
import esphome.components.logger  # noqa: F401
