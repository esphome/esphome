# prometheus

## Date, time and datetime metrics (#9941)

`scripts/prometheus/prom-datetime.yaml` is a D1 mini with `web_server`, `prometheus` and three optimistic template
datetime entities; `prom-datetime-tz.yaml` adds an sntp time component with `HST10` (the zone applies without a sync).
`datetime_metrics.py HOST` prints the rows of `/metrics`, sets the entities over the API to 2040 values (past 2038 on
purpose) and prints them again with their UTC reading. Dates are local midnight in ms, times ms since midnight,
datetimes ms since the epoch, so a datetime set as an instant reads the same in both zones. It exits 1 when a time
or datetime row does not hold the value it set (dates depend on the zone and are only printed).
