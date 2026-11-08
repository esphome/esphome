# Per period PWM capture on the same chip (#15438)

`scripts/pwm_capture/` (ESP32-C6): `pwmcap.hpp` routes the `ledc` pad through the GPIO matrix into RMT receive at
80 MHz (ping pong buffers, `en_partial_rx`, `mem_block_symbols: 96`) and a PCNT unit, checks every period in the IRAM
safe RMT interrupt and logs a `WIN` line every 10 s (high time and period histograms, minimum and maximum, level
changes, spikes, reversals). `common.yaml` needs `include_builtin_idf_components: [esp_driver_rmt, esp_driver_pcnt]`
and `CONFIG_RMT_ISR_IRAM_SAFE` and `CONFIG_PCNT_ISR_IRAM_SAFE`; build it with `-s name NAME -s ps light` (or `none`).
`run.sh NAME SECONDS [--every 5 --transition 1]` (env `TREE`, `PORT`, `SC`, `PY`) resets and captures while
`apiclient.py` holds an API connection and optionally sends light transitions; `summ.py` totals the windows.

- LEDC and RMT share PLL_F80M, so a 20 kHz 11 bit output reads exactly 4000 ticks per period with no jitter.
- Result: no glitch in about 10 million periods with power save `light` or `none`; #15718 (LEDC in IRAM) changed
  nothing. Positive control: a lambda that changes the duty for a few periods is caught every time.
- Window boundaries follow RMT chunks (about 1000 periods), so `n` alternates between 199584 and 200592; sum over
  the run.
