# LOS201 — Embedded Linux Systems

> **FPT University · Embedded Linux Systems (LOS201)**  
> **Ubuntu 24.04 · ARMv7-A / Cortex-A9 · Linux 5.15 · QEMU Versatile Express (`vexpress-a9`)**

Repository tổng hợp quá trình học tập và thực hành **Embedded Linux Systems** với **3 bài Lab và 2 Assignment**. Nội dung đi từ xây dựng một hệ thống Linux nhúng trên ARM, phát triển kernel module và giao tiếp userspace–kernel, đến debugging, profiling và ứng dụng giám sát cảm biến đa luồng.

Mã nguồn chủ yếu sử dụng **C, Shell, Makefile/Kbuild, Linux Kernel APIs, POSIX Threads** và công cụ cross-compilation `arm-linux-gnueabihf-`.

**Đi nhanh đến từng bài:** [Lab 01](embedded_lab1/README.md) · [Lab 02](embedded_lab2/README.md) · [Lab 03](embedded_lab3/README.md) · [Assignment 01](embedded-linux-assignment01/README.md) · [Assignment 02](embedded-linux-assignment02/README.md)

---

## 1. Tổng quan repository

| Bài | Chủ đề | Nội dung chính | Chi tiết |
|---|---|---|---|
| **Lab 01** | Kernel Build & System Boot | Linux 5.15, U-Boot, BusyBox, Device Tree, initramfs, boot QEMU | [README Lab 01](embedded_lab1/README.md) |
| **Lab 02** | Linux Device Drivers | Character device, `read()/write()`, mutex, procfs/sysfs, MTD & JFFS2 | [README Lab 02](embedded_lab2/README.md) |
| **Lab 03** | Debugging & Profiling | Remote GDB, Valgrind, `strace`, `perf`, `cyclictest`, `stress-ng` | [README Lab 03](embedded_lab3/README.md) |
| **Assignment 01** | IOCTL & Misc Drivers | IOCTL character driver, buffer modes, driver statistics, atomic counter | [README Assignment 01](embedded-linux-assignment01/README.md) |
| **Assignment 02** | Sensor Monitoring System | Kernel sensor, 4 POSIX threads, circular buffer, message queues, alerts, logging | [README Assignment 02](embedded-linux-assignment02/README.md) |

### Cấu trúc thư mục

| Thư mục | Vai trò |
|---|---|
| [`embedded_lab1/`](embedded_lab1/) | Xây dựng kernel, root filesystem và nền tảng QEMU ARM |
| [`embedded_lab2/`](embedded_lab2/) | Driver ký tự, procfs/sysfs và thực hành MTD/JFFS2 |
| [`embedded_lab3/`](embedded_lab3/) | Công cụ debugging, memory analysis và performance profiling |
| [`embedded-linux-assignment01/`](embedded-linux-assignment01/) | Hai bài tập kernel driver: IOCTL và Misc Counter |
| [`embedded-linux-assignment02/`](embedded-linux-assignment02/) | Hệ thống giám sát nhiệt độ/độ ẩm đa luồng |

Các thư mục con như `driver/`, `app/`, `apps/`, `rootfs/`, `output/`, `logs/` và `bao_cao/` được sử dụng tùy nội dung mỗi bài. README trong từng thư mục chứa hướng dẫn và kết quả chi tiết hơn.

## 2. Môi trường phát triển

| Thành phần | Cấu hình |
|---|---|
| **Host** | Ubuntu 24.04 LTS, x86_64 |
| **Target** | ARMv7-A, ARM Cortex-A9 |
| **Emulator** | QEMU `vexpress-a9`, 2 vCPU, 512 MiB RAM |
| **Kernel** | Linux 5.15.0 |
| **Toolchain** | `arm-linux-gnueabihf-gcc` (hard-float ABI) |
| **Bootloader** | U-Boot 2022.04 |
| **Userspace** | BusyBox 1.35.0, initramfs |
| **Development** | C, Shell, GCC, Makefile, Kbuild, Git |
| **Analysis** | GDB/GDBserver, Valgrind, `strace`, `perf`, `cyclictest`, `stress-ng` |

**Lưu ý Host/Guest:** Module `.ko` được cross-compile cho ARM và chạy trong **QEMU Guest**. Các phép đo Valgrind, perf hoặc latency thực hiện trên **Ubuntu Host** phải được ghi nhận riêng; chúng không tương đương hoàn toàn với phép đo trên phần cứng ARM thật.

---

## 3. Lab 01 — Linux Kernel Build & Boot

**Mục tiêu:** Tạo một hệ thống Embedded Linux tối giản và khởi động thành công trên bo mạch ARM Cortex-A9 giả lập.

**Nội dung chính:**

- Cấu hình và cross-compile **Linux kernel 5.15** cho ARMv7-A.
- Tạo **kernel image (`zImage`)** và **Device Tree Blob (`vexpress-v2p-ca9.dtb`)**.
- Làm quen với **U-Boot 2022.04** và các tham số khởi động kernel.
- Biên dịch **BusyBox 1.35.0**, xây dựng **initramfs** và cấu hình tiến trình `init`.
- Boot với **QEMU**, kiểm tra serial console, CPU, filesystem và các lệnh Linux cơ bản.

**Kết quả:** Hệ thống boot được Linux 5.15 trên QEMU ARMv7 và truy cập BusyBox shell. Kernel, Device Tree và root filesystem này tiếp tục được sử dụng cho các bài sau.

**[Xem hướng dẫn và kết quả Lab 01](embedded_lab1/README.md)**

## 4. Lab 02 — Linux Device Drivers & MTD/JFFS2

**Mục tiêu:** Phát triển kernel module và thực hiện trao đổi dữ liệu giữa userspace và kernel space.

**Nội dung chính:**

- Xây dựng **character device driver** với các thao tác `open()`, `read()`, `write()` và `release()`.
- Tạo giao diện **`/dev/lab2`** để chương trình userspace đọc/ghi dữ liệu.
- Sử dụng **mutex** để bảo vệ buffer và dữ liệu được chia sẻ.
- Xuất trạng thái qua **`/proc/lab2_info`** và các thuộc tính **sysfs**.
- Cross-compile `lab2_driver.ko` cho ARM, tích hợp module vào initramfs và kiểm tra bằng script.
- Thực hành **MTD/JFFS2** với NAND giả lập (`nandsim`): kiểm tra MTD, tạo image, mount và kiểm tra dữ liệu.

**Kết quả:** Phần character device và procfs/sysfs đã chạy PASS trong QEMU. Thí nghiệm MTD/JFFS2 được thực hiện riêng trên Ubuntu Host và cần xác định đúng NAND giả lập trước các lệnh ghi/xóa.

> **Cảnh báo MTD:** Tuyệt đối không mặc định `/dev/mtd0` là NAND giả lập. Phải kiểm tra `/proc/mtd` và `mtdinfo` trước khi dùng `flash_erase` hoặc `nandwrite` để tránh tác động đến flash thật.

**[Xem hướng dẫn và kết quả Lab 02](embedded_lab2/README.md)**

## 5. Lab 03 — Debugging & Performance Analysis

**Mục tiêu:** Phát hiện lỗi chương trình, kiểm tra bộ nhớ, phân tích system calls và đánh giá scheduling latency.

| Phần | Công cụ | Nội dung |
|---|---|---|
| **A** | GDB, GDBserver | Remote debugging ứng dụng ARM trên QEMU |
| **B** | Valgrind | Kiểm tra memory leaks và xác minh bản sửa |
| **C** | `strace`, `perf` | Phân tích system calls, CPU cycles, instructions và hotspots |
| **D** | `cyclictest`, `stress-ng` | Đo độ trễ lập lịch ở trạng thái bình thường và có tải |

**Kết quả tiêu biểu đã ghi nhận:**

- GDB xác định **3 lỗi**: array out-of-bounds, use-after-free và integer overflow.
- Bản ứng dụng đã sửa chạy thành công trên Ubuntu Host, `exit code = 0`, không có lỗi được AddressSanitizer/UndefinedBehaviorSanitizer báo trong lần kiểm thử.
- Valgrind: **16,900 bytes / 250 blocks definitely lost** ở bản lỗi; sau sửa còn **0 bytes definitely lost**.
- `strace` và `perf` cung cấp syscall statistics, CPU metrics và thông tin về hotspots.
- `cyclictest` được chạy cả khi bình thường và khi chịu tải từ `stress-ng`.

**Phạm vi:** Thực nghiệm latency trên Host/QEMU chưa chứng minh khả năng **hard real-time** hay hiệu năng của một board ARM vật lý.

**[Xem hướng dẫn và kết quả Lab 03](embedded_lab3/README.md)**

## 6. Assignment 01 — IOCTL & Misc Device Drivers

**Mục tiêu:** Xây dựng hai Linux kernel drivers với giao diện và cơ chế quản lý trạng thái khác nhau.

### Task 1 — IOCTL Character Driver

- Tạo **`asgn1_driver.ko`** và thiết bị **`/dev/asgn1`**.
- Cài đặt **4 lệnh IOCTL**: `RESET_BUFFER`, `GET_STATS`, `SET_MODE`, `GET_VERSION`.
- Hỗ trợ **Echo Mode** (giữ nguyên dữ liệu) và **Kernel Mode** (thêm prefix `[KERNEL]`).
- Quản lý buffer và thống kê số lần mở/đọc/ghi; sử dụng mutex và xử lý mã lỗi phù hợp.
- Kiểm thử bằng một chương trình userspace và script tự động.

### Task 2 — Misc Counter Driver

- Tạo **`counter_driver.ko`** và thiết bị **`/dev/counter`** bằng `misc_register()`.
- Sử dụng `atomic_t` kết hợp mutex để quản lý bộ đếm.
- Mỗi lượt đọc hợp lệ trả về giá trị hiện tại rồi tăng counter; ghi số nguyên hợp lệ để đặt lại.
- Xuất thống kê qua **`/proc/counter_info`**; kiểm tra input sai và quá trình unload.

**Kết quả kiểm thử đã lưu:**

| Bài kiểm tra | PASS | FAIL |
|---|---:|---:|
| Task 1 — IOCTL test application | 18 | 0 |
| Task 1 — Shell test script | 5 | 0 |
| Task 2 — Counter test script | 8 | 0 |
| **Tổng** | **31** | **0** |

**[Xem hướng dẫn và kết quả Assignment 01](embedded-linux-assignment01/README.md)**

## 7. Assignment 02 — Sensor Monitoring System

**Mục tiêu:** Phát triển hệ thống giám sát nhiệt độ/độ ẩm mô phỏng, kết hợp **Linux kernel driver** và ứng dụng **multithreading** trên ARM.

**Các thành phần chính:**

- **Sensor driver `sms_sensor_driver.ko`:** mô phỏng mẫu nhiệt độ/độ ẩm; cung cấp **`/dev/sms_sensor`**, thống kê **`/proc/sms_stats`** và cấu hình sampling interval bằng `ioctl()`.
- **Sensor Thread:** đọc dữ liệu định kỳ và đo scheduling jitter.
- **Processor Thread:** tính rolling average 10 mẫu, phát hiện bất thường và phát thông điệp.
- **Alert Thread:** xử lý cảnh báo **`TEMP_HIGH`**, **`HUMID_HIGH`**, **`TEMP_SPIKE`**.
- **Logger Thread:** lưu sample và alert vào **`/tmp/sms.log`**.
- **Synchronization & IPC:** circular buffer, mutex, condition variable và POSIX message queues.
- **Scheduling:** đánh giá `SCHED_OTHER` và `SCHED_FIFO`, đo average/max/P99 jitter và histogram.

**Kết quả tiêu biểu đã báo cáo:**

- Lượt chạy **60 giây, chu kỳ 500 ms**: **118 samples produced**, **2 dropped**, **0 buffer overflows**; `read_count = 118` sau chạy.
- Valgrind trên **Host với mock sensor**: các lần chạy được ghi nhận **0 memory errors**, không còn heap blocks bị mất khi thoát.
- Ở bài thử **50 ms có CPU load** trên QEMU, median P99 jitter qua ba lượt giảm từ **575 µs** (`SCHED_OTHER`) xuống **336 µs** (`SCHED_FIFO`). Đây là kết quả trong điều kiện thí nghiệm, không phải bảo đảm trên phần cứng thật.

**Demo bắt buộc trong đề:** boot QEMU và load module; chạy `sms_app -v`; giải thích alert; kiểm tra log; dừng ứng dụng và xem summary. Phần quay video phải được kiểm tra riêng, không được coi là đã hoàn thành chỉ từ log chương trình.

**[Xem hướng dẫn và kết quả Assignment 02](embedded-linux-assignment02/README.md)**

---

## 8. Quick Start — QEMU ARM

### Cài đặt công cụ cơ bản trên Ubuntu Host

```bash
sudo apt update
sudo apt install -y \
    build-essential git make cpio gzip \
    gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
    qemu-system-arm gdb-multiarch
```

### Boot hệ thống nền từ Lab 01

**Chạy trong Ubuntu Host** (terminal có dấu `$`):

```bash
cd ~/LOS201/embedded_lab1

# Kiểm tra file trước khi chạy.
ls -lh output/zImage output/vexpress-v2p-ca9.dtb output/initramfs.cpio.gz

qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 \
    -kernel output/zImage \
    -dtb output/vexpress-v2p-ca9.dtb \
    -initrd output/initramfs.cpio.gz \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init" \
    -nographic -no-reboot
```

**Chạy trong QEMU Guest** (terminal có dấu `/ #`):

```sh
uname -m
uname -r
cat /proc/cpuinfo
mount
```

**Thoát QEMU `-nographic`:** nhấn **Ctrl+A**, thả tay rồi nhấn **X**.

Với Lab 02 và hai Assignment, cần sử dụng **initramfs riêng phù hợp với bài**, sau đó chạy module/test script tương ứng. Không mặc định initramfs Lab 01 đã chứa tất cả driver hoặc ứng dụng.

## 9. Nhóm thực hiện

**FPT University — Embedded Linux Systems (LOS201)**  
**Giảng viên:** Phạm Thế Vinh

| STT | Thành viên | Mã số sinh viên |
|---:|---|---|
| 1 | **Lê Tiến Thành** | **SE201799** |
| 2 | Lê Tấn Lộc | SE201412 |
| 3 | Trần Đình Khôi Nguyên | SE200558 |
| 4 | Trần Trương Tuấn Vũ | SE196467 |

## 10. Ghi chú và tài liệu

- Các kết quả ở README này được tổng hợp từ **các lượt thực hành/log đã ghi nhận**. Kết quả khi chạy lại có thể khác do thay đổi kernel, build, CPU load hoặc cấu hình QEMU.
- Luôn phân biệt chương trình chạy trên **Host x86_64** với chương trình/kernel module chạy trên **Guest ARMv7**.
- Hệ thống sử dụng **sensor giả lập**, không phải dữ liệu đo từ cảm biến vật lý.
- Không dùng kết quả QEMU làm bằng chứng về độ trễ phần cứng hoặc chuẩn hard real-time.
- Mọi thông tin build/test chuyên sâu, mã nguồn và tài liệu báo cáo nằm trong README hoặc thư mục của từng Lab/Assignment.

---

*Educational repository for **LOS201 — Embedded Linux Systems**, FPT University. Built and documented for coursework, experimentation and reproducible learning.*
