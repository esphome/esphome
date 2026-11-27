# Automation code generation A/B (#21978 to #21980)

A change to how actions, conditions or scripts are generated must not change what runs or in which order.
`scripts/automation_trace/automation-trace-common.yaml` (a package) logs a marker `M <case> <values>` at every step.
It covers:
- merged action chains, delays inside chains, nested `if`/`else` with and/or/not/xor, `choose` with `else`;
- `while` with and without a delay, nested and dynamic `repeat`, `wait_until` and `for:`;
- `single`, `restart`, `queued` and `parallel` scripts with parameters, a self `script.stop` mid-chain;
- `on_value_range`, alarm panel triggers, and parallel scripts with `max_runs` 2, 3 and 4 that execute themselves
  from an `if` branch.

`run.sh LABEL` writes the per-run file (board, Wi-Fi from `secrets.yaml`, the package), builds and flashes over
`PORT`, takes the device address from the serial log and runs `drive.py HOST OUT`. The driver records the boot run,
a "Run All" press and API-driven triggers (Chain Button twice, Num 70 and 20, Sw on and off, Sel c and a). The
runner keeps the part from `DRIVER press Run All` on in `out-at<LABEL>.trace`; diff dev against each PR head. The
boot run is partly missed while the API connects, so it is left out of the diff.

- Pass: the Run All pass reaches `M done`, no `SHOULD_NOT_RUN` or `NOT_REACHED` marker appears, and the trace is the
  same on both builds.
- The interleaving of the `queued` and `parallel` scripts with later steps is timing dependent, but it was identical
  across builds and runs.
- To check a real device config as well, OTA the PR build and toggle one light and one switch from an API client,
  returning each to its original state, and compare states and logs with the base build. Get the owner's approval
  before flashing a device that is in use.
