#!/bin/busybox sh
export PATH=/sbin:/bin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
echo KDASICS_BOOT=READY
fail()
{
    echo KDASICS_TEST=FAIL
    exec /bin/busybox sh
}
for round in 1 2; do
    for test in data_bound_refill fault_recovery maincall_service; do
        insmod /modules/${test}_utmod.ko trust=0 || fail
        insmod /modules/${test}_tmod.ko trust=1 || fail
        rmmod ${test}_tmod || fail
        rmmod ${test}_utmod || fail
        echo KDASICS_MODULE=${test}:ROUND=${round}:PASS
    done
done
echo KDASICS_TEST=PASS
exec /bin/busybox sh
