# sensor and number on_value_range A/B (#21814, #21817, #21818)

`scripts/value_range/value-range-common.yaml` (a package) has one template sensor with seven triggers (constant
bounds, one-sided, a lambda bound on `x`, a bound from another sensor's state that stays NaN until step 22, both
bounds NaN, and an identical twin) and one template number with three triggers. It feeds them 30 values 200 ms apart
(on the bounds, just past them, repeats, NaN, ±inf, a moving bound), then syncs preferences and restarts, then
sends 50, 95 and 50 to check the restored in-range flag. Every fire logs its step and `x`.

- `run.sh bk72xx|esp8266|esp32 LABEL` writes the per-build file with a fresh `tag`, builds, flashes, captures 90 s
  (through the restart) and writes `out-rng<kind><LABEL>.seq`; diff dev against each PR head.
- The sequence primes every trigger first (bound 0, value -500, bound NaN): the in-range flags are stored in flash,
  so without priming a fresh build starts from whatever the previous run left, and the first fires differ.
- The phase global resets when `tag` changes, so each reflash runs the whole sequence.
- Log line numbers differ between trees; the runner strips them before writing the `.seq` file.
- Results: https://github.com/esphome/esphome/pull/21818#issuecomment-6491846503
