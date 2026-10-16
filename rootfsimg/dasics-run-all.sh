#!/bin/sh
# Run the existing guest tests; do not change their assertions or exit status.
loaded_modules=

cleanup()
{
    cleanup_status=0
    for module in $loaded_modules; do
        if grep -q "^$module " /proc/modules; then
            rmmod "$module" || cleanup_status=1
        fi
        if grep -q "^$module " /proc/modules; then
            echo "DASICS_MODULE_CLEANUP=FAIL module=$module"
            cleanup_status=1
        fi
    done
    if [ "$cleanup_status" -eq 0 ]; then
        loaded_modules=
    fi
    return "$cleanup_status"
}

finish()
{
    test_status=$?
    trap - 0 HUP INT TERM
    if cleanup; then
        echo DASICS_ALL_CLEANUP=PASS
    else
        echo DASICS_ALL_CLEANUP=FAIL
        test_status=1
    fi
    if [ "$test_status" -eq 0 ]; then
        echo DASICS_ALL_RESULT=PASS
    else
        echo "DASICS_ALL_RESULT=FAIL status=$test_status"
    fi
    exit "$test_status"
}

trap finish 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
echo DASICS_ALL_BEGIN

# Do not unload modules owned by a previous or concurrent manual session.
[ -r /proc/modules ] || exit 1
for suite in data_bound_refill fault_recovery maincall_service; do
    if grep -q "^${suite}_" /proc/modules; then
        echo "DASICS_ALL_BUSY suite=$suite"
        exit 1
    fi
done

if /dasics/suite; then
    echo LINUX_DASICS_RESULT=PASS
else
    echo LINUX_DASICS_RESULT=FAIL
    exit 1
fi
if /dasics/dasics-test-utimer --expect=off &&
   /dasics/dasics-test-utimer --expect=on -dasics; then
    echo LINUX_UTIMER_RESULT=PASS
else
    echo LINUX_UTIMER_RESULT=FAIL
    exit 1
fi

for round in 1 2; do
    for suite in data_bound_refill fault_recovery maincall_service; do
        echo "DASICS_KERNEL_CASE_BEGIN name=$suite round=$round"
        insmod "/modules/${suite}_utmod.ko" trust=0 || exit 1
        loaded_modules="${suite}_utmod"
        insmod "/modules/${suite}_tmod.ko" trust=1 || exit 1
        loaded_modules="${suite}_tmod $loaded_modules"
        cleanup || exit 1
        echo "DASICS_KERNEL_CASE_END name=$suite round=$round status=0"
    done
done
echo 'DASICS_KERNEL_SUITE_RESULT=PASS rounds=2'
