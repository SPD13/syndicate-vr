#!/bin/sh
# Push decomp/auto to origin (the SPD13 fork, never upstream) whenever it
# moves. Exits when run.py is no longer running.
cd "$(dirname "$0")/../.." || exit 1
while :; do
    if [ "$(git rev-parse decomp/auto)" != "$(git rev-parse origin/decomp/auto 2>/dev/null)" ]; then
        echo "[$(date +%T)] push $(git log --oneline -1 decomp/auto)"
        git push -q origin decomp/auto || echo "[$(date +%T)] push failed"
    fi
    powershell -NoProfile -Command "if (Get-CimInstance Win32_Process -Filter \"Name like 'python%'\" | Where-Object { \$_.CommandLine -match 'run\.py' }) { exit 0 } else { exit 1 }" || { echo "run.py ended"; exit 0; }
    sleep 120
done
