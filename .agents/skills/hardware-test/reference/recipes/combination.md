# combination

## NaN from the last source (#22510)

A combination sensor defers each update and uses the value of the source that published last. A NaN from that
source drops the whole update, so when testing NaN handling keep the last-publishing source finite, and compare
rows one update late. Heap churn per update: see `heap_alloc.md`.
