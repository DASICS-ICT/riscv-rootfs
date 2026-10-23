#!/bin/sh
# Automatic and manual kernel tests share one owner-aware runner.
finish()
{
    status=$?
    trap - 0 HUP INT TERM
    if [ "$status" -eq 0 ]; then
        echo DASICS_ALL_RESULT=PASS
    else
        echo "DASICS_ALL_RESULT=FAIL status=$status"
    fi
    exit "$status"
}
trap finish 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

echo DASICS_ALL_BEGIN
if /dasics/suite; then
    echo LINUX_DASICS_RESULT=PASS
else
    echo LINUX_DASICS_RESULT=FAIL
    exit 1
fi
# The user suite already includes both UTimer modes.
/dasics/kernel-runner all --rounds=2 || exit $?
echo DASICS_ALL_CLEANUP=PASS
