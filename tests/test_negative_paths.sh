#!/usr/bin/env bash
# Regression test: PluMA must exit non-zero when a pipeline config
# references a plugin name that doesn't match any registered plugin.
# Guards against src/main.cxx's "no suitable language for plugin"
# path silently continuing (and exiting 0) instead of aborting.
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

if [ ! -x ./pluma ]; then
    echo "SKIP: ./pluma binary not found (build with 'scons' first)"
    exit 0
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

cat > "$TMPDIR/config.txt" <<'EOF'
Plugin NoSuchPluginXYZ123 inputfile foo outputfile bar
EOF

./pluma "$TMPDIR/config.txt" > "$TMPDIR/out.log" 2>&1
STATUS=$?

if [ "$STATUS" -eq 0 ]; then
    echo "FAIL: pluma exited 0 for a config referencing a nonexistent plugin (expected nonzero exit)"
    cat "$TMPDIR/out.log"
    exit 1
fi

echo "PASS: pluma exited $STATUS (nonzero) for a nonexistent plugin reference, as expected"
exit 0
