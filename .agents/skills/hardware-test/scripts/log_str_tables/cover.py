"""cover.py PR BOARD: which table names of each component appear in the PR log, and whether PR and base match.

Reads `l<PR>p<BOARD>.norm` (PR build) and `l<PR>b<BOARD>.norm` (merge base) from the current directory.
"""

import pathlib
import re
import sys

T = {
    "1": {
        "ds3231": (
            r"square wave|Square wave",
            ["1 Hz", "1.024 kHz", "4.096 kHz", "8.192 kHz", "unknown"],
        ),
        "es9038q2m_katana filter": (
            r"Filter shape",
            [
                "linear_phase_fast",
                "linear_phase_slow",
                "minimum_phase_fast",
                "minimum_phase_slow",
                "apodizing",
                "hybrid",
                "brick_wall",
            ],
        ),
        "es9038q2m_katana deemphasis": (
            r"De-emphasis",
            ["bypass", "32khz", "44_1khz", "48khz"],
        ),
        "lis2dw12 power": (
            r"Power Mode",
            [
                "High Performance",
                "Low Power 1 (12-bit)",
                "Low Power 2",
                "Low Power 3",
                "Low Power 4",
                "Unknown",
            ],
        ),
        "lis2dw12 rate": (
            r"Data Rate",
            [
                "1.6 Hz",
                "12.5 Hz",
                "25 Hz",
                "50 Hz",
                "100 Hz",
                "200 Hz",
                "400 Hz",
                "800 Hz",
                "1600 Hz",
                "Unknown",
            ],
        ),
        "lis2dw12 range": (r"  Range", ["±2g", "±4g", "±8g", "±16g", "Unknown"]),
        "lis2dw12 bandwidth": (
            r"Filter Bandwidth",
            ["ODR/2", "ODR/4", "ODR/10", "ODR/20", "Unknown"],
        ),
        "npm1300": (
            r"^\[C\]\[npm1300\]:   [A-Z]",
            ["BUCK1:", "BUCK2:", "LDO1:", "LDO2:"],
        ),
        "sy6970": (
            r"Watchdog Timeout|watchdog timeout",
            ["Disabled", "40s", "80s", "160s", "Unknown"],
        ),
        "tmc22xx": (
            r"\[tmc22xx\]",
            [
                "Driver reset",
                "Driver error",
                "Charge pump undervoltage",
                "Overtemperature prewarning",
                "Overtemperature shutdown",
                "Short to ground on phase A",
                "Short to ground on phase B",
                "Low side short on phase A",
                "Low side short on phase B",
                "Open load on phase A",
                "Open load on phase B",
                "Temperature above 120C",
                "Temperature above 143C",
                "Temperature above 150C",
                "Temperature above 157C",
                "Unknown",
            ],
        ),
    },
    "2": {
        "adc sampling": (
            r"Sampling mode",
            ["average", "minimum", "maximum", "unknown"],
        ),
        "adc attenuation": (
            r"Attenuation",
            ["0 dB", "2.5 dB", "6 dB", "12 dB", "Auto", "Unknown Attenuation"],
        ),
        "dfrobot_sen0609": (
            r"Giving up on|No command for",
            [
                "none",
                "stop",
                "start",
                "restart",
                "factory reset",
                "write",
                "save",
                "read",
            ],
        ),
        "ens210": (
            r"status failure",
            [
                "Status OK",
                "Invalid data",
                "CRC error",
                "I2C error - communication with ENS210 failed!",
                "Unknown",
            ],
        ),
        "mhz19": (
            r"etection range",
            ["default", "0-2000 ppm", "0-5000 ppm", "0-10000 ppm"],
        ),
        "mlx90393": (
            r"failed to apply|verify",
            [
                "gain",
                "resolution",
                "oversampling",
                "digital filtering",
                "temperature oversampling",
                "temperature compensation",
                "hallconf",
                "error",
                "unknown",
            ],
        ),
        "selec_meter": (
            r"while reading|Skipping update",
            ["main block", "serial number", "DG sensing", "idle"],
        ),
    },
    "3": {
        "display_menu_base": (
            r"\(Type: ",
            [
                "MENU_ITEM_LABEL",
                "MENU_ITEM_MENU",
                "MENU_ITEM_BACK",
                "MENU_ITEM_SELECT",
                "MENU_ITEM_NUMBER",
                "MENU_ITEM_SWITCH",
                "MENU_ITEM_COMMAND",
                "MENU_ITEM_CUSTOM",
                "UNKNOWN",
            ],
        ),
        "mitsubishi_cn105": (
            r"transition",
            [
                "Not connected",
                "Connecting",
                "Connected",
                "UpdatingStatus",
                "StatusUpdated",
                "DeferredStatusRequest",
                "ScheduleNextStatusUpdate",
                "WaitingForScheduledStatusUpdate",
                "ApplyingSettings",
                "SettingsApplied",
                "ReadTimeout",
                "Unknown",
            ],
        ),
        "remote_base IRA211": (
            r"Received IRA211",
            ["TEMP_UP", "TEMP_DOWN", "MODE", "FAN", "POWER", "SYNC", "UNKNOWN"],
        ),
        "uln2003": (r"Step Mode", ["FULL STEP", "HALF STEP", "WAVE DRIVE", "UNKNOWN"]),
        "voice_assistant": (
            r"State changed|Desired state",
            [
                "IDLE",
                "START_MICROPHONE",
                "STARTING_MICROPHONE",
                "WAIT_FOR_VAD",
                "WAITING_FOR_VAD",
                "START_PIPELINE",
                "STARTING_PIPELINE",
                "STREAMING_MICROPHONE",
                "STOP_MICROPHONE",
                "STOPPING_MICROPHONE",
                "AWAITING_RESPONSE",
                "STREAMING_RESPONSE",
                "RESPONSE_FINISHED",
                "UNKNOWN",
            ],
        ),
    },
}


def read(path: str) -> list[str]:
    return pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()


def has(name: str, lines: list[str]) -> bool:
    pat = r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])"
    return any(re.search(pat, ln) for ln in lines)


pr, board = sys.argv[1], sys.argv[2]
if pr not in T:
    sys.exit(f"no name tables for group {pr}; known: {', '.join(T)}")
p = read(f"l{pr}p{board}.norm")
b = read(f"l{pr}b{board}.norm")
for comp, (pat, names) in T[pr].items():
    lines = [ln for ln in p if re.search(pat, ln)]
    blines = [ln for ln in b if re.search(pat, ln)]
    seen = [n for n in names if has(n, lines)]
    missing = [n for n in names if n not in seen]
    print(
        f"{comp:30s} lines {len(lines):3d}/{len(blines):3d} same={lines == blines} "
        f"names {len(seen)}/{len(names)} missing={missing}"
    )
