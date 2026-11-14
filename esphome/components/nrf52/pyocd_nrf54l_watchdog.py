"""pyOCD user script that stops the watchdog of an nRF54L chip after connecting.

A running watchdog keeps counting while pyOCD holds the CPU halted, unless the firmware paused it
for the debugger, and resets the chip part way through flashing. Zephyr lets it be stopped.
Passed to 'pyocd flash --script'; pyOCD runs it, not ESPHome.
"""

WDT31 = 0x50109000
WDT_TASKS_STOP = 0x004
WDT_TSEN = 0x520
WDT_TSEN_KEY = 0x6E524635


def did_init_target(target) -> None:
    target.write32(WDT31 + WDT_TSEN, WDT_TSEN_KEY)
    target.write32(WDT31 + WDT_TASKS_STOP, 1)
    target.write32(WDT31 + WDT_TSEN, 0)
