#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
# Tiny run_kselftest.sh — inspired by Linux tools/testing/selftests/run_kselftest.sh
#
# Usage (on mini-os ash):
#   sh /kselftests/run_kselftest.sh
#   sh /kselftests/run_kselftest.sh yield/yield_test
#   /kselftests/yield/yield_test
#
# Note: mini-os has no shebang (#!) loader yet, so scripts must be
# started with `sh …`. ELF test binaries can be exec'd directly.
#
# Each test binary speaks TAP on its own. This runner summarizes
# pass/fail from exit status (KSFT_PASS=0).

# BusyBox ash: no dirname(1) — strip the last /component by hand.
case "$0" in
*/*) BASE=${0%/*} ;;
*)   BASE=. ;;
esac
cd "$BASE" || exit 1

if [ "$#" -gt 0 ]; then
	TESTS="$*"
elif [ -f kselftest-list.txt ]; then
	TESTS=$(cat kselftest-list.txt)
else
	echo "Bail out! no tests listed" >&2
	exit 1
fi

pass=0
fail=0

for t in $TESTS; do
	echo "== $t =="
	if [ ! -x "$t" ]; then
		echo "FAIL: missing $t"
		fail=$((fail + 1))
		continue
	fi
	if ./"$t"; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
	fi
	echo
done

echo "kselftest summary: pass:$pass fail:$fail"
[ "$fail" -eq 0 ]
