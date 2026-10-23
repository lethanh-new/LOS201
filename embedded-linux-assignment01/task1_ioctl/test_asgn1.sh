#!/bin/sh

PASS=0
FAIL=0
DEVICE=/dev/asgn1
MODULE=/asgn1_driver.ko
APP=/test_asgn1_app

check() {
    if [ "$1" -eq 0 ]; then
        echo "[PASS] $2"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $2"
        FAIL=$((FAIL + 1))
    fi
}

echo "========== ASSIGNMENT 01 - TASK 1 =========="
echo "Kernel: $(uname -r)"

echo "========== LOAD MODULE =========="
if ! insmod "$MODULE"; then
    echo "[FAIL] Cannot load asgn1_driver"
    exit 1
fi

grep -q '241 asgn1' /proc/devices
check "$?" "Major 241 registered"

if [ ! -c "$DEVICE" ]; then
    mknod "$DEVICE" c 241 0
fi

test -c "$DEVICE"
check "$?" "Device /dev/asgn1 exists"

echo "========== RUN IOCTL TESTS =========="
"$APP"
APP_RC=$?
check "$APP_RC" "All ioctl test cases"

echo "========== UNLOAD MODULE =========="
rmmod asgn1_driver
check "$?" "Unload asgn1_driver"

if grep -q '241 asgn1' /proc/devices; then
    check 1 "Major number cleanup"
else
    check 0 "Major number cleanup"
fi

echo "========== DRIVER KERNEL LOG =========="
dmesg | grep -i asgn1 | tail -60

echo "========== SCRIPT SUMMARY =========="
echo "PASSED: $PASS"
echo "FAILED: $FAIL"

[ "$FAIL" -eq 0 ]
