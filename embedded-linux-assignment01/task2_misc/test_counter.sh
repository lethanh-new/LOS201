#!/bin/sh

PASS=0
FAIL=0

check() {
    if [ "$1" = "$2" ]; then
        echo "[PASS] $3"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $3 - expected='$2', actual='$1'"
        FAIL=$((FAIL + 1))
    fi
}

echo "========== ASSIGNMENT 01 - TASK 2 =========="
echo "Kernel: $(uname -r)"

echo "========== LOAD MODULE =========="
if ! insmod /counter_driver.ko; then
    echo "[FAIL] insmod counter_driver"
    exit 1
fi

echo "========== DEVICE NODE =========="
ls -l /dev/counter
lsmod | grep counter_driver
grep counter /proc/misc

if [ ! -c /dev/counter ]; then
    echo "[FAIL] /dev/counter not found"
    exit 1
fi

echo "========== PROCFS BEFORE WRITE =========="
cat /proc/counter_info

echo "========== AUTO INCREMENT =========="
A=$(cat /dev/counter)
B=$(cat /dev/counter)

check "$A" "0" "First read returns 0"
check "$B" "1" "Second read returns 1"

echo "========== SET 100 =========="
printf '100\n' > /dev/counter
echo "========== PROCFS AFTER WRITE =========="
cat /proc/counter_info
C=$(cat /dev/counter)
check "$C" "100" "Set counter to 100"

echo "========== RESET 0 =========="
printf '0\n' > /dev/counter
echo "========== PROCFS AFTER RESET =========="
cat /proc/counter_info
D=$(cat /dev/counter)
check "$D" "0" "Reset counter to 0"

echo "========== INVALID NEGATIVE =========="
if printf '%s\n' '-1' > /dev/counter; then
    echo "[FAIL] Negative value accepted"
    FAIL=$((FAIL + 1))
else
    echo "[PASS] Negative value rejected"
    PASS=$((PASS + 1))
fi

echo "========== INVALID STRING =========="
if printf '%s\n' 'abc' > /dev/counter; then
    echo "[FAIL] Invalid string accepted"
    FAIL=$((FAIL + 1))
else
    echo "[PASS] Invalid string rejected"
    PASS=$((PASS + 1))
fi

echo "========== FINAL PROCFS =========="
cat /proc/counter_info

echo "========== UNLOAD MODULE =========="
if rmmod counter_driver; then
    echo "[PASS] rmmod counter_driver"
    PASS=$((PASS + 1))
else
    echo "[FAIL] rmmod counter_driver"
    FAIL=$((FAIL + 1))
fi

if [ ! -e /proc/counter_info ]; then
    echo "[PASS] procfs cleanup"
    PASS=$((PASS + 1))
else
    echo "[FAIL] procfs cleanup"
    FAIL=$((FAIL + 1))
fi

echo "========== KERNEL LOG =========="
dmesg | grep -i counter | tail -40

echo "========== TEST SUMMARY =========="
echo "PASSED: $PASS"
echo "FAILED: $FAIL"

[ "$FAIL" -eq 0 ]
