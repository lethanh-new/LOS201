# Assignment 01 — Linux Device Drivers on ARMv7

**LOS201 · Embedded Linux Systems · FPT University**  
**Platform:** Linux 5.15.0 · ARM Cortex-A9 · QEMU `vexpress-a9` · BusyBox 1.35  
**Language:** C · Linux Kernel Module (Kbuild) · ARM cross-compilation

This assignment implements and verifies two Linux kernel drivers on an ARMv7 Embedded Linux system. **Task 1** extends a character device with four `ioctl()` commands, two data modes, synchronized statistics, and an ARM userspace test application. **Task 2** implements an auto-incrementing counter using the Linux miscdevice framework and publishes runtime counters through procfs.

Both modules were built against the Linux 5.15 target kernel and tested inside QEMU. The repository also documents the technical comparison of character, block, and network devices, and the design decisions behind synchronization, I/O semantics, and resource cleanup.

> **Scope:** The actual implementation and runtime test results below refer to Task 1 and Task 2. The remaining conceptual/analysis portions of the assignment are covered in the accompanying report; they are not presented as additional kernel modules.

## Contents

- [1. Highlights and verified results](#1-highlights-and-verified-results)
- [2. Target environment](#2-target-environment)
- [3. Repository structure](#3-repository-structure)
- [4. System architecture](#4-system-architecture)
- [5. Task 1 — IOCTL character driver](#5-task-1--ioctl-character-driver)
- [6. Task 2 — Misc counter driver](#6-task-2--misc-counter-driver)
- [7. Build instructions](#7-build-instructions)
- [8. Boot and run on QEMU](#8-boot-and-run-on-qemu)
- [9. Test evidence](#9-test-evidence)
- [10. Driver comparison and design notes](#10-driver-comparison-and-design-notes)
- [11. Troubleshooting](#11-troubleshooting)
- [12. Limitations and references](#12-limitations-and-references)

## 1. Highlights and verified results

| Component | Verified outcome | Result |
|---|---|---|
| Task 1 — ARM IOCTL test application | 18 passed, 0 failed | **PASS** |
| Task 1 — module/test script | 5 passed, 0 failed | **PASS** |
| Task 2 — counter test script | 8 passed, 0 failed | **PASS** |
| Target kernel | Linux 5.15.0 on ARMv7 | **PASS** |
| Task 1 device | `/dev/asgn1`, major `241`, minor `0` in the tested run | **PASS** |
| Task 2 device | `/dev/counter`, major `10`, minor `125` in the tested run | **PASS** |
| Module lifecycle | Load, test, unload, and cleanup verified | **PASS** |

**Recorded results:** `18 + 5 + 8 = 31` passing test assertions across the two application/script test suites. These are **previously captured QEMU results**, not an automatic guarantee for every fresh build or kernel configuration.

### Main technical topics

- ARMv7-A cross-compilation and out-of-tree Linux kernel modules
- `register_chrdev()` versus `misc_register()`
- `struct file_operations`: `open`, `read`, `write`, `release`, `unlocked_ioctl`
- Userspace/kernel-space transfers using `copy_from_user()` and `copy_to_user()`
- Command encoding with `_IO`, `_IOR`, `_IOW`
- Mutexes, `atomic_t`, per-open state, file offsets, and EOF semantics
- `seq_file`/procfs diagnostics and safe module cleanup

## 2. Target environment

| Component | Configuration |
|---|---|
| Development host | Ubuntu Linux, x86_64 |
| Target ISA | ARMv7-A, hard-float ABI |
| Emulated board | QEMU `vexpress-a9` |
| CPU | ARM Cortex-A9, 2 vCPUs |
| RAM | 512 MiB |
| Kernel | Linux `5.15.0` |
| Root filesystem | BusyBox initramfs |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Kbuild options | `ARCH=arm`, `CROSS_COMPILE=arm-linux-gnueabihf-` |
| Observed module vermagic | `5.15.0 SMP mod_unload ARMv7 p2v8` |

The project is located at:

```text
/home/letienthanh/LOS201/embedded-linux-assignment01
```

The previously built Lab 1 kernel and device tree are expected at:

```text
~/embedded_lab1/output/zImage
~/embedded_lab1/output/vexpress-v2p-ca9.dtb
```

The matching kernel build tree is normally located at:

```text
~/embedded_lab1/kernel/linux-5.15
```

The Linux kernel running on the **Ubuntu host** is not interchangeable with the **ARM guest** kernel. Do not build these ARM modules against `/lib/modules/$(uname -r)/build` on the x86_64 host.

## 3. Repository structure

```text
embedded-linux-assignment01/
├── README.md                       # This document
├── task1_ioctl/
│   ├── asgn1_driver.c              # Character device + IOCTL implementation
│   ├── asgn1_ioctl.h               # Shared IOCTL definitions and stats structure
│   ├── asgn1_ioctl_test.c          # Userspace IOCTL test source (if retained)
│   ├── test_asgn1_app              # ARM test executable (built artifact)
│   ├── test_asgn1.sh               # Task 1 automated test script
│   ├── Makefile                    # Kernel module Kbuild wrapper
│   └── asgn1_driver.ko             # ARM kernel module (built artifact)
├── task2_misc/
│   ├── counter_driver.c            # Misc device + procfs implementation
│   ├── test_counter.sh             # Task 2 automated test script
│   ├── Makefile
│   └── counter_driver.ko           # ARM kernel module (built artifact)
├── output/
│   └── initramfs_asgn1_final.cpio.gz  # Final tested combined initramfs
├── logs/                            # Captured build, boot, and test output
└── bao_cao/                         # Report and supporting evidence, if present
```

The listing shows the **logical project layout and known files**. Generated files (`*.ko`, ELF binaries, logs, archives) may be absent from a fresh source-only checkout and need rebuilding. Use `find . -maxdepth 3 -type f` to inspect the exact contents of your checkout.

The final initramfs used during the recorded test placed these files at the **guest root**:

```text
/asgn1_driver.ko
/test_asgn1_app
/test_asgn1.sh
/counter_driver.ko
/test_counter.sh
```

These are guest filesystem paths, **not** paths to execute from an Ubuntu terminal.

## 4. System architecture

```mermaid
flowchart LR
    U[ARM userspace / test programs]
    V[VFS / Linux system calls]
    A[Task 1: asgn1_driver]
    C[Task 2: counter_driver]
    B[Driver buffer + stats + mutex]
    K[Atomic counter + mutex]
    P[/proc/counter_info]

    U -->|open / read / write / ioctl| V
    V -->|/dev/asgn1| A
    V -->|/dev/counter| C
    A <--> B
    C <--> K
    U -->|read procfs| P
    P --> K
```

Task 1 uses a registered character-device major and a shared IOCTL header. Task 2 uses `misc_register()` with the conventional misc major (`10`) and exposes its own procfs statistics. The procfs interface **reads state**; it does not call `/dev/counter`'s `read()` handler and therefore does not auto-increment the counter.

## 5. Task 1 — IOCTL character driver

**Implementation:** `task1_ioctl/asgn1_driver.c`  
**Shared interface:** `task1_ioctl/asgn1_ioctl.h`  
**Device:** `/dev/asgn1`

### IOCTL commands

All commands use magic character `'A'` (`0x41`). On the tested ARM/Linux configuration:

| Command | Encoding | Observed ioctl value | Function |
|---|---|---|---|
| `ASGN1_RESET_BUFFER` | `_IO('A', 1)` | `0x4101` | Clear stored data and resettable statistics |
| `ASGN1_GET_STATS` | `_IOR('A', 2, struct asgn1_stats)` | `0x80144102` | Copy a five-field statistics snapshot to userspace |
| `ASGN1_SET_MODE` | `_IOW('A', 3, int)` | `0x40044103` | Select one of two write modes |
| `ASGN1_GET_VERSION` | `_IOR('A', 4, char[64])` | `0x80404104` | Return the bounded driver version string |

The five statistics fields are:

```text
open_count
write_count
read_count
buffer_len
last_write_size
```

**Reset semantics:** `RESET_BUFFER` clears the data and resettable counters but **preserves `open_count`**, which tracks historical successful opens since loading the module.

### Write modes

| Mode | Behavior | Example |
|---|---|---|
| `1` — Echo | Store exactly the input payload | `hello` → `hello` |
| `0` — Kernel | Prepend `[KERNEL]` to the payload | `world` → `[KERNEL]world` |

The test confirmed `buffer_len = 5` after writing five bytes in Echo mode and `buffer_len = 13` after writing five bytes in Kernel mode. `last_write_size` remained `5` in both cases because it measures the original userspace input.

### Error handling and synchronization

- Unknown IOCTL commands return `-ENOTTY` (tested with `0xdeadbeef`).
- Invalid mode values return `-EINVAL` (tested with mode `2`).
- Failed userspace-memory transfers return `-EFAULT`.
- A mutex protects shared buffer state and relevant statistics against concurrent access.
- The registered device was observed at major `241`, minor `0`, with a class device under `/sys/class/asgn1_class/asgn1`.

### Observed test output

```text
Echo read: 'hello'
Kernel read: '[KERNEL]world'
Version: asgn1_driver v1.0

========== TEST SUMMARY ==========
PASSED: 18
FAILED: 0
```

At the end of the test, the buffer is empty and all resettable counters return to zero; `open_count` remains nonzero by design.

## 6. Task 2 — Misc counter driver

**Implementation:** `task2_misc/counter_driver.c`  
**Device:** `/dev/counter`  
**Statistics:** `/proc/counter_info`

This module registers a **miscdevice**, which is a simpler registration mechanism for a character device. It uses shared major `10`, while the minor is allocated dynamically (observed as `125` in the captured run).

### Counter contract

| Operation | Behavior |
|---|---|
| `cat /dev/counter` | Returns the current decimal value, then increments the counter once |
| `printf '100\n' > /dev/counter` | Sets the counter to `100` |
| `printf '0\n' > /dev/counter` | Resets the counter to `0` |
| Writing `-1` or `abc` | Rejects input with `-EINVAL` |
| `cat /proc/counter_info` | Displays `counter_value`, `total_reads`, `total_writes` without incrementing |

The implementation uses `atomic_t` for individual counter operations and a mutex to protect the multi-step “snapshot → successful read → increment → update statistics” transaction. Per-open state and `simple_read_from_buffer()` ensure that a `cat` finishes at EOF instead of reading indefinitely.

### Observed procfs checkpoints

| Observation | `counter_value` | `total_reads` | `total_writes` |
|---|---:|---:|---:|
| Initial state | 0 | 0 | 0 |
| Immediately after writing `100` | 100 | 2 | 1 |
| Immediately after writing `0` | 0 | 3 | 2 |
| Final recorded state | 1 | 4 | 2 |

The values differ between checkpoints because a successful read through `/dev/counter` advances the counter; reading the procfs stats does not.

The automated test previously ended with **8 PASS / 0 FAIL**, including invalid-input checks and removal of the procfs entry after unloading the module.

## 7. Build instructions

All commands in this section run on the **Ubuntu host** (`$` prompt), not in the QEMU guest (`/ #`).

### 7.1. Prerequisites

```bash
sudo apt update
sudo apt install -y \
  build-essential make gcc-arm-linux-gnueabihf \
  binutils-arm-linux-gnueabihf \
  qemu-system-arm cpio gzip file kmod
```

Verify the target kernel tree and boot artifacts:

```bash
export KDIR="$HOME/embedded_lab1/kernel/linux-5.15"
test -f "$KDIR/Makefile" || { echo "Missing Linux 5.15 kernel source"; exit 1; }
ls -lh "$HOME/embedded_lab1/output/zImage" \
       "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb"
```

> `KDIR` must reference the **configured and built** kernel tree matching the QEMU kernel. A different kernel version/configuration may prevent `insmod` from working.

### 7.2. Build Task 1 kernel module

```bash
cd "$HOME/LOS201/embedded-linux-assignment01/task1_ioctl"
make -C "$KDIR" M="$PWD" \
  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

file asgn1_driver.ko
modinfo asgn1_driver.ko | grep -E 'vermagic|filename'
```

### 7.3. Build Task 2 kernel module

```bash
cd "$HOME/LOS201/embedded-linux-assignment01/task2_misc"
make -C "$KDIR" M="$PWD" \
  ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

file counter_driver.ko
modinfo counter_driver.ko | grep -E 'vermagic|filename'
```

Expected target architecture: `ELF 32-bit ... ARM`, with a `vermagic` matching the target Linux kernel.

### 7.4. Build the Task 1 ARM userspace test application

If the userspace test source is present under `task1_ioctl/`, build it with the ARM toolchain and the shared IOCTL header:

```bash
cd "$HOME/LOS201/embedded-linux-assignment01/task1_ioctl"

# Confirm the exact C filename used in this checkout.
ls -l *test*.c asgn1_ioctl.h 2>/dev/null

# For the commonly used source name:
arm-linux-gnueabihf-gcc -static -O2 -Wall -Wextra \
  -I. asgn1_ioctl_test.c -o test_asgn1_app

file test_asgn1_app
```

If your checkout uses a differently named test source, substitute its actual filename. The **recorded final guest image** used `/test_asgn1_app`, and this README follows that tested path.

### 7.5. Using the tested initramfs or rebuilding it

The simplest reproducible route is to boot the previously verified archive:

```text
output/initramfs_asgn1_final.cpio.gz
```

To package updated modules or scripts, start from a **known working BusyBox ARM initramfs**, extract its contents into a temporary directory, copy in the new artifacts, and create a **new** `newc` archive. Do not add raw files to the end of an already gzip-compressed archive.

Example using the Lab 2 initramfs as the base (as in the recorded assignment setup):

```bash
cd "$HOME/LOS201/embedded-linux-assignment01" || exit 1
BASE="$HOME/LOS201/embedded_lab2/output/initramfs_assignment1.cpio.gz"
OUT="$PWD/output/initramfs_asgn1_rebuilt.cpio.gz"

[ -f "$BASE" ] || { echo "Base initramfs not found: $BASE"; exit 1; }
[ -f task1_ioctl/asgn1_driver.ko ] || exit 1
[ -f task1_ioctl/test_asgn1_app ] || exit 1
[ -f task2_misc/counter_driver.ko ] || exit 1

WORK="$(mktemp -d)" || exit 1
mkdir -p "$WORK/rootfs" output

(
  cd "$WORK/rootfs" || exit 1
  gzip -dc "$BASE" | cpio -idmu --no-absolute-filenames --quiet || exit 1
) || { rm -rf "$WORK"; exit 1; }

install -m 644 task1_ioctl/asgn1_driver.ko "$WORK/rootfs/asgn1_driver.ko"
install -m 755 task1_ioctl/test_asgn1_app "$WORK/rootfs/test_asgn1_app"
install -m 755 task1_ioctl/test_asgn1.sh "$WORK/rootfs/test_asgn1.sh"
install -m 644 task2_misc/counter_driver.ko "$WORK/rootfs/counter_driver.ko"
install -m 755 task2_misc/test_counter.sh "$WORK/rootfs/test_counter.sh"

(
  cd "$WORK/rootfs" || exit 1
  find . -print0 | cpio --null -o -H newc --quiet | gzip -9
) > "$OUT"

rm -rf "$WORK"
ls -lh "$OUT"
```

This **optional repackaging recipe is supplied for reproducibility and should be retested after use**. The PASS counts in Section 1 were captured with the previously verified final image, not with every possible rebuilt archive.

## 8. Boot and run on QEMU

### 8.1. Boot the final image (Ubuntu Host `$`)

```bash
cd "$HOME/LOS201/embedded-linux-assignment01" || exit 1

qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -kernel "$HOME/embedded_lab1/output/zImage" \
  -dtb "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb" \
  -initrd output/initramfs_asgn1_final.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init' \
  -nographic -no-reboot
```

When prompted with `Please press Enter to activate this console`, press **Enter**. The ARM guest shell has a `/ #` prompt.

### 8.2. Verify the guest (QEMU `/ #`)

```sh
uname -m
uname -r
ls -lh /asgn1_driver.ko /test_asgn1_app /test_asgn1.sh \
       /counter_driver.ko /test_counter.sh
```

Expected architecture/kernel:

```text
armv7l
5.15.0
```

### 8.3. Run Task 1 (QEMU `/ #`)

```sh
sh /test_asgn1.sh
echo "TASK1_EXIT_CODE=$?"
```

Expected outcome from the recorded test:

```text
PASSED: 18
FAILED: 0
TASK1_EXIT_CODE=0
```

The shell wrapper also verifies major registration, the device node, and cleanup. **Do not manually `insmod /asgn1_driver.ko` before running a script that already loads the module**, otherwise a duplicate insertion can fail.

### 8.4. Run Task 2 (QEMU `/ #`)

```sh
sh /test_counter.sh
echo "TASK2_EXIT_CODE=$?"
```

Expected: `8 PASS`, `0 FAIL`, exit code `0`.

### 8.5. Inspect driver logs and module state

```sh
lsmod
grep -E 'asgn1|counter' /proc/devices
cat /proc/misc | grep counter || true
dmesg | grep -E 'asgn1|counter_driver' | tail -35
```

Successful test scripts **unload their modules** as part of cleanup, so a missing `/dev/asgn1` or `/proc/counter_info` **after the complete test** can be the *expected* result, not a failure.

### 8.6. Manual Task 1 inspection (separate from automated test)

If the test script has already unloaded Task 1, load it manually:

```sh
insmod /asgn1_driver.ko
ls -l /dev/asgn1
cat /proc/devices | grep asgn1
ls -l /sys/class/asgn1_class/asgn1

# Use the ARM test application for IOCTL-based control.
/test_asgn1_app

rmmod asgn1_driver
```

Use this only for an independent manual test; the ARM C application may perform its own resets and state transitions. The main recommended evidence remains the automated test script.

### Exit QEMU

For QEMU `-nographic`, press **Ctrl+A**, release both keys, then press **X**. This exits the emulator rather than only the shell.

## 9. Test evidence

Recorded Task 1 observations:

```text
RESET_IOCTL   = 0x4101
STATS_IOCTL   = 0x80144102
MODE_IOCTL    = 0x40044103
VERSION_IOCTL = 0x80404104

Echo read: 'hello'
Kernel read: '[KERNEL]world'
Version: asgn1_driver v1.0

PASSED: 18
FAILED: 0
```

Recorded Task 2 observations:

```text
Device: /dev/counter
Procfs: /proc/counter_info
Major/minor in tested run: 10:125
Test script: 8 PASS / 0 FAIL
```

Relevant evidence files in the documented workflow include `test_asgn1_output.txt`, `test_counter_output.txt`, `dmesg_full.txt`, and the Task 1/Task 2 build logs. File locations can differ between versions of the project; inspect the local `logs/` and report directories for the actual retained evidence.

To save a **new** console transcript, start QEMU inside a terminal recorder on the host (for example `script -f logs/asgn1_qemu_session.txt`), then run both test scripts in the guest and exit the recorder after QEMU closes.

## 10. Driver comparison and design notes

| Aspect | Standard character device (Task 1) | Misc character device (Task 2) |
|---|---|---|
| Registration | `register_chrdev()` + class/device | `misc_register()` |
| Major | Dedicated `241` in tested build | Shared `10` |
| Minor | `0` | Dynamically allocated |
| Interface | `read`, `write`, custom `ioctl` | `read`, `write`, procfs status |
| Main state | Byte buffer + five statistics fields | Atomic counter + read/write totals |
| Synchronization | Mutex for shared state | `atomic_t` plus mutex for multi-step transactions |
| Diagnostics | `GET_STATS`, version command, kernel logs | `/proc/counter_info`, kernel logs |

### Character vs. block vs. network devices

- **Character:** Streams or records, typically accessed through `/dev` and `file_operations`; suitable for UART-style devices and simple sensors.
- **Block:** Random-access sectors, request queues, and block I/O; suitable for eMMC, SD cards, and storage devices.
- **Network:** Packets and frames through the network stack and `net_device_ops`; suitable for Ethernet or CAN (SocketCAN).

A **miscdevice is still a character device**; it is not a fourth I/O class.

### Engineering choices

- `mutex` protects read-modify-write groups and logically related fields, not merely isolated increments.
- `atomic_t` protects individual atomic operations, but does not automatically make an entire snapshot-and-update transaction atomic.
- `read()` must honor file position and return `0` at EOF to prevent commands such as `cat` from reading forever.
- Kernel code must not directly dereference arbitrary userspace pointers; use the kernel uaccess helpers.
- Every successful registration/allocation should have a corresponding failure rollback and unload cleanup.

## 11. Troubleshooting

| Symptom | Cause / resolution |
|---|---|
| `Exec format error` when running an ARM binary on Ubuntu | The ARM executable belongs in the QEMU guest. Check `file` and `uname -m`. |
| `Invalid module format` on `insmod` | Check ARM architecture, kernel version/configuration, and `modinfo ... | grep vermagic`. |
| `/test_asgn1.sh: not found` | Verify the exact initramfs used for boot and inspect the archive contents. |
| `/dev/asgn1` missing | Check if `asgn1_driver` is loaded; run the provided script or inspect major registration. |
| `insmod: File exists` / module already loaded | A script or previous command may already have inserted the module. Check `lsmod` before retrying. |
| `/proc/counter_info` missing after a test | The counter script intentionally unloads the module; reload for an independent inspection. |
| `cat /dev/counter` unexpectedly changes the value | By design, each successful first read per open increments the counter once. |
| Wrong counter in procfs after a test read | Read `/proc/counter_info` **before** the next `/dev/counter` read when comparing checkpoints. |
| `Permission denied` on device node | Check its permissions and run test commands as root in the guest, or adjust permissions only in the test image. |
| Kernel panic: cannot execute init | Verify the ARM BusyBox binary, `/sbin/init` symlink, initramfs format, and boot arguments. |

## 12. Limitations and references

- These results describe **QEMU-based functional validation**, not testing against physical ARM hardware or a production device driver.
- Major `241` and misc minor `125` reflect the **tested run**; hard-coding these values may be inappropriate for a general-purpose deployment.
- Recorded PASS counts do not establish race-freedom under arbitrary multi-process stress or prove correctness for every invalid pointer and resource-failure path.
- The final initramfs is derived from an existing course Lab 2 environment; some Lab 2 startup messages may remain visible while booting Assignment 1.
- Any edits to the driver code, scripts, or rootfs require rebuilding/repackaging and rerunning the tests before claiming the same results.

**Course material and local evidence**

- Official assignment specification: `ASSIGNMENT01_EmbeddedLinux.docx` (where supplied with the coursework)
- Assignment 01 technical report and screenshots in the report directory
- Recorded QEMU test outputs, build logs, and kernel messages in the `logs/` directory or associated evidence package
- Linux kernel documentation: <https://docs.kernel.org/>
- QEMU system emulator documentation: <https://www.qemu.org/docs/master/system/>

---

**Project context:** Embedded Linux Systems (LOS201), FPT University, Ho Chi Minh City, 2026.  
**Team:** Lê Tiến Thành · Lê Tấn Lộc · Trần Đình Khôi Nguyên · Trần Trương Tuấn Vũ.
