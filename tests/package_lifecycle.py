"""Explicit on-device DEB lifecycle test. Run as root on the test Pi, with no host streaming.
Temporarily stops/removes the service, preserves configuration and restores it running.
Usage: sudo taskset -c 0,1 python3 package_lifecycle.py package.deb report.json
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

if os.geteuid() != 0 or len(sys.argv) != 3:
    raise SystemExit('Run as root with package.deb and output.json')
package = Path(sys.argv[1]).resolve(strict=True)
report_path = Path(sys.argv[2]).resolve()
service = 'pi-aoip.service'
results = []

def run(*args, check=True):
    completed = subprocess.run(args, capture_output=True, text=True)
    results.append(dict(command=list(args), code=completed.returncode,
                        stdout=completed.stdout, stderr=completed.stderr))
    if check and completed.returncode:
        raise RuntimeError(f'{args}: {completed.stderr}')
    return completed

def active():
    return subprocess.run(['systemctl', 'is-active', '--quiet', service]).returncode == 0

def ready():
    for _ in range(100):
        pid = subprocess.check_output(['systemctl','show','-p','MainPID','--value',service], text=True).strip()
        if active() and pid.isdigit() and int(pid) and len(list(Path(f'/proc/{pid}/task').glob('*/status'))) >= 4:
            run('piaoip-doctor')
            return pid
        time.sleep(.1)
    raise RuntimeError('Service did not start its control/source threads')

paths = [Path('/etc/piaoip/peer.conf'), Path('/var/lib/piaoip/profile.txt')]
def hashes():
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

original = hashes()
passed = False
try:
    previous_pid = ready()
    run('dpkg','-i',str(package))
    if ready() == previous_pid:
        raise RuntimeError('Active service was not restarted by upgrade')
    if hashes() != original:
        raise RuntimeError('Upgrade changed configuration')
    run('systemctl','stop',service)
    run('dpkg','-i',str(package))
    if active():
        raise RuntimeError('Upgrade started a manually stopped service')
    if run('piaoip-doctor', check=False).returncode == 0:
        raise RuntimeError('Doctor did not report an inactive service')
    run('systemctl','start',service)
    ready()
    run('dpkg','-r','piaoip-rpi4')
    if Path('/usr/lib/piaoip/aoip_peer_rpi4').exists() or hashes() != original:
        raise RuntimeError('Remove did not preserve config/remove executable')
    run('dpkg','-i',str(package))
    ready()
    run('systemctl','is-enabled',service)
    if hashes() != original:
        raise RuntimeError('Reinstall changed configuration')
    passed = True
finally:
    if not Path('/usr/lib/piaoip/aoip_peer_rpi4').exists():
        run('dpkg','-i',str(package), check=False)
    if not active():
        run('systemctl','start',service, check=False)
    report_path.write_text(json.dumps(dict(passed=passed, original_hashes=original,
                                          final_hashes=hashes(), steps=results), indent=2))
    print(f'DEB_LIFECYCLE_PASS={passed}; report={report_path}', flush=True)
