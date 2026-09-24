"""Read-only one-second CPU samples on the Pi; run under taskset -c 0,1."""
import json
from pathlib import Path
import statistics
import sys
import time

seconds = int(sys.argv[1])
output = Path(sys.argv[2])
if not 1 <= seconds <= 300:
    raise SystemExit('seconds must be 1..300')

def counters():
    result = {}
    for line in Path('/proc/stat').read_text().splitlines():
        fields = line.split()
        if fields[0] in ('cpu0', 'cpu1', 'cpu2', 'cpu3'):
            # guest/guest_nice are already included in user/nice.
            values = list(map(int, fields[1:9]))
            result[fields[0]] = (sum(values), values[3] + values[4])
    return result

samples = []
previous = counters()
for _ in range(seconds):
    time.sleep(1)
    current = counters()
    sample = {'unix_time': time.time(), 'busy_percent': {}}
    for cpu, (total, idle) in current.items():
        elapsed = total - previous[cpu][0]
        idle_elapsed = idle - previous[cpu][1]
        sample['busy_percent'][cpu] = 100 * (elapsed - idle_elapsed) / elapsed if elapsed else 0
    samples.append(sample)
    previous = current
summary = {cpu: {'mean_percent': statistics.mean(s['busy_percent'][cpu] for s in samples),
                 'peak_1s_percent': max(s['busy_percent'][cpu] for s in samples)} for cpu in current}
output.write_text(json.dumps({'seconds': seconds, 'summary': summary, 'samples': samples}, indent=2))
print(json.dumps(summary), flush=True)
