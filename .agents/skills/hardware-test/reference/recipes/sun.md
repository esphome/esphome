# sun on_sunrise and on_sunset without waiting for the sun (#21803)

`scripts/sun/sun-common.yaml` (a package) puts the observer on the equator and, once SNTP has synced, searches the
longitudes for one where the sun stands near -5° and rises as the observer moves east. An interval then moves the
longitude by 0.125° every 5 s, which sweeps the elevation from -5° to +6° and back at about 1.5° a minute, reversing
above +6° and below -5°. Five triggers (sunrise at the default, 2° and 4°, sunset at the default and -2°) fire from
the real 60 s elevation poll; each logs the elevation it fired at and counts in `fires`, logged every 30 s.

- The sweep stops after one full pass and logs `sweep done`. `run.sh esp32|esp8266 LABEL` builds, flashes and
  captures up to 20 minutes (a pass takes about 16), then `check.py` wants `sweep done` and the counter at 5 with no
  trigger firing twice. Run dev and the PR on the same board; two boards can run in parallel.
- At 1.5° a minute a crossing fires up to about 1.7° past its elevation, one poll late at most.
- The seconds past the minute of the fire lines show the poll phase: separate pollers fire at different seconds,
  one shared poll fires every trigger in the same second.
- `id(sun_id)` is a pointer in lambdas (`->`). Judge by the `fires` counter: the classic devkit's serial log can
  garble single lines.
- Results: https://github.com/esphome/esphome/pull/21803#issuecomment-6490173072
