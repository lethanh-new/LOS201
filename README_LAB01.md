# LAB 01 — Kernel Configuration & Embedded Linux Boot

**LOS201 · ARMv7-A/Cortex-A9 · Linux 5.15 · U-Boot 2022.04 · BusyBox 1.35 · QEMU `vexpress-a9`**

[← Workspace LOS201](../README.md) · [LAB 02 →](../embedded_lab2/README.md)

## Mục lục

- [1. Mục tiêu và phạm vi](#1-mục-tiêu-và-phạm-vi)
- [2. Hệ thống và kiến trúc boot](#2-hệ-thống-và-kiến-trúc-boot)
- [3. Các file của Lab 1](#3-các-file-của-lab-1)
- [4. Phần A — Configure và build Linux kernel](#4-phần-a--configure-và-build-linux-kernel)
- [5. Phần B — U-Boot](#5-phần-b--u-boot)
- [6. Phần C — BusyBox và initramfs](#6-phần-c--busybox-và-initramfs)
- [7. Boot trên QEMU và kiểm chứng](#7-boot-trên-qemu-và-kiểm-chứng)
- [8. Kết quả quan sát](#8-kết-quả-quan-sát)
- [9. Troubleshooting](#9-troubleshooting)
- [10. Checklist và tài liệu](#10-checklist-và-tài-liệu)

## 1. Mục tiêu và phạm vi

LAB 01 xây dựng hệ thống Linux tối giản chạy trên **ARM Cortex-A9 được QEMU giả lập**, bao gồm kernel ARM, Device Tree, BusyBox userspace và initramfs. Phần U-Boot minh họa vai trò bootloader trước khi kernel nhận quyền điều khiển.

Sau Lab này cần hiểu và thực hiện được:

1. Phân biệt **Host x86_64** và **Target ARMv7**; cross-compile với toolchain `arm-linux-gnueabihf-`.
2. Chọn đúng `vexpress_defconfig`, kiểm tra Kconfig, build `zImage`, `dtbs`, modules.
3. Hiểu ý nghĩa DTB và boot arguments `console=`, `rdinit=`, `mem=`.
4. Cấu hình/build BusyBox **statically linked** và tạo rootfs tối giản.
5. Đóng gói `newc cpio` → gzip, boot QEMU đến `/ #`.
6. Dùng U-Boot kiểm tra `printenv`, `bdinfo`, `fdt`, nạp và khởi chạy kernel.

## 2. Hệ thống và kiến trúc boot

| Thành phần | Cấu hình đã thực hành |
|---|---|
| Host | Ubuntu x86_64 |
| Target | QEMU `vexpress-a9`, ARM Cortex-A9 |
| Target Linux | 5.15.0, ARMv7-A |
| RAM / CPU giả lập | 512 MiB, thường `-smp 2` |
| Kernel image | `output/zImage` |
| Hardware description | `output/vexpress-v2p-ca9.dtb` |
| Root filesystem | `rootfs/initramfs` → `output/initramfs.cpio.gz` |
| Userspace | BusyBox 1.35.0, static ARM ELF |
| Bootloader thực hành | U-Boot 2022.04 |

```mermaid
flowchart TD
    P[QEMU tạo machine + cấp RAM] --> R{Cách boot}
    R -->|Bootloader| U[U-Boot: chọn ảnh, bootargs, địa chỉ RAM]
    R -->|QEMU -kernel trực tiếp| K[Linux zImage + DTB]
    U --> K
    K --> I[Giải nén initramfs]
    I --> INIT[/sbin/init — PID 1]
    INIT --> RCS[/etc/init.d/rcS]
    RCS --> FS[mount proc, sysfs, tmpfs]
    FS --> SH[BusyBox shell: / #]
```

**Phân biệt:** Khi QEMU dùng `-kernel output/zImage`, QEMU hỗ trợ nạp kernel **không đi qua U-Boot**. Muốn chứng minh chuỗi U-Boot → kernel phải thực sự boot qua U-Boot rồi chạy `bootz`.

## 3. Các file của Lab 1

```text
embedded_lab1/
├── README.md
├── configs/
│   ├── kernel.config               # .config của Linux kernel (nếu đã lưu)
│   └── busybox.config              # .config của BusyBox (nếu đã lưu)
├── rootfs/
│   └── initramfs/
│       ├── bin/ sbin/ usr/
│       ├── etc/inittab
│       ├── etc/init.d/rcS
│       ├── dev/ proc/ sys/ tmp/
│       └── sbin/init               # thường là BusyBox symlink
└── output/
    ├── zImage
    ├── vexpress-v2p-ca9.dtb
    └── initramfs.cpio.gz
```

Đây là **những file kỳ vọng** từ quá trình thực hành. Source Linux/U-Boot/BusyBox có thể được lưu ngoài repo vì dung lượng lớn. Để tìm nhanh:

```bash
cd ~/LOS201/embedded_lab1
find . -maxdepth 3 -type f | sort | head -80
find output -maxdepth 1 -type f -exec file {} \;
```

## 4. Phần A — Configure và build Linux kernel

### 4.1. Chuẩn bị công cụ và source

Cần toolchain ARM, `make`, `bc`, `flex`, `bison`, `libncurses-dev`, `libssl-dev`, `libelf-dev`; chi tiết ở [README gốc](../README.md#5-chuẩn-bị-công-cụ).

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export LAB1_DIR="$HOME/LOS201/embedded_lab1"

# THAY đường dẫn này bằng thư mục Linux source 5.15 thật trên máy:
export KERNEL_SRC="$HOME/linux-5.15"

test -d "$KERNEL_SRC/arch/arm" || echo 'Hãy sửa KERNEL_SRC trước khi build'
```

### 4.2. Nạp cấu hình phù hợp `vexpress-a9`

```bash
cd "$KERNEL_SRC"
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- vexpress_defconfig

grep -E '^CONFIG_(ARCH_VEXPRESS|CPU_V7|BLK_DEV_INITRD|SERIAL_AMBA_PL011|SMP)=' .config
```

Không dùng nhầm `x86_64 defconfig` chỉ vì Host là Ubuntu x86_64. Tùy chọn chính cần kiểm tra:

| Option / nhóm | Vai trò |
|---|---|
| `CONFIG_ARCH_VEXPRESS` | Nền tảng ARM Versatile Express |
| ARMv7 / Cortex-A9 | CPU target |
| `CONFIG_BLK_DEV_INITRD` | Hỗ trợ initrd/initramfs |
| `CONFIG_SERIAL_AMBA_PL011` và console | Serial console `ttyAMA0` |
| `CONFIG_DEVTMPFS`, `PROC_FS`, `SYSFS` | Hệ thống tệp/thiết bị runtime |
| `CONFIG_SMP` | Cho phép nhiều CPU; số CPU online còn phụ thuộc QEMU/DTB |
| `CONFIG_MODULES` | LAB 02 cần module hỗ trợ load/unload |

Chỉnh thêm qua menuconfig nếu cần:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig
```

### 4.3. Build và lưu hiện vật

```bash
cd "$KERNEL_SRC"
make -j"$(nproc)" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage dtbs modules

mkdir -p "$LAB1_DIR/output" "$LAB1_DIR/configs"
cp arch/arm/boot/zImage "$LAB1_DIR/output/zImage"
cp arch/arm/boot/dts/vexpress-v2p-ca9.dtb "$LAB1_DIR/output/vexpress-v2p-ca9.dtb"
cp .config "$LAB1_DIR/configs/kernel.config"

file "$LAB1_DIR/output/zImage"
file "$LAB1_DIR/output/vexpress-v2p-ca9.dtb"
```

**Kiểm tra `arch/arm/boot/dts/...`** trong kernel 5.15 trước khi copy. Không dùng DTB của board khác.

## 5. Phần B — U-Boot

### 5.1. Tại sao cần U-Boot?

Bootloader chuẩn bị thông tin platform, chọn ảnh kernel, DTB, initrd/initramfs, thiết lập `bootargs` và chuyển quyền thực thi cho kernel. Với QEMU, bước đưa U-Boot vào RAM được máy ảo mô phỏng; không có Boot ROM vật lý để quan sát.

Các lệnh tại dấu nhắc **U-Boot `=>`**:

```text
version
help
bdinfo
printenv
help bootz
fdt header
```

### 5.2. Các địa chỉ RAM trong lần kiểm thử của nhóm

Trên `vexpress-a9`, DRAM được ánh xạ bắt đầu khoảng `0x60000000`. Một lần boot đã kiểm chứng sử dụng:

| Vật thể | Địa chỉ |
|---|---|
| zImage | `0x62000000` |
| initramfs | `0x64000000` |
| DTB | `0x66000000` |

Ví dụ thao tác **chỉ sau khi ba ảnh đã được nạp thật vào các địa chỉ đó**:

```text
=> setenv bootargs 'console=ttyAMA0,115200 rdinit=/sbin/init'
=> fdt addr 0x66000000
=> fdt header
=> bootz 0x62000000 0x64000000:<INITRD_SIZE_HEX> 0x66000000
```

Trên **Host**, lấy chính xác kích thước file đã nạp (không dùng cố định kích thước từ máy khác):

```bash
cd ~/LOS201/embedded_lab1
stat -c '%s bytes' output/initramfs.cpio.gz
printf '0x%x\n' "$(stat -c %s output/initramfs.cpio.gz)"
```

Thay `<INITRD_SIZE_HEX>` bằng kết quả Hex thật. Đoạn `bootz` **không** tự nạp file vào RAM; cách nạp có thể dùng loader của QEMU, TFTP hoặc phương thức thực tế mà môi trường đã cấu hình.

> **Lỗi từng gặp:** DTB đặt ở `0x60000000` có lần gây `FDT_ERR_BADSTRUCTURE`. Việc dùng `0x66000000` và xác minh `fdt header` đã giúp tránh địa chỉ chồng lấn trong thí nghiệm; không coi một địa chỉ là đúng cho mọi bo mạch.

## 6. Phần C — BusyBox và initramfs

### 6.1. Biên dịch BusyBox ARM static

Cấu hình đã thực hành dùng BusyBox 1.35.0, hard-float ABI:

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export BUSYBOX_SRC="$HOME/busybox-1.35.0"  # sửa nếu source ở nơi khác

cd "$BUSYBOX_SRC"
make defconfig
make menuconfig
```

Trong **Settings → Build Options** bật **Build static binary (no shared libs)**; cross-compiler prefix tương ứng `arm-linux-gnueabihf-`. Nếu thiết lập CFLAGS riêng phải phù hợp target Cortex-A9 và hard-float ABI. Sau đó:

```bash
make -j"$(nproc)" CROSS_COMPILE=arm-linux-gnueabihf-
make CROSS_COMPILE=arm-linux-gnueabihf- install
file _install/bin/busybox
```

Kết quả cần là **ARM ELF32, statically linked**. Nếu là dynamically linked, phải đưa trình nạp động và đúng ARM libraries vào rootfs trước khi boot.

### 6.2. Root filesystem cơ bản

```text
/
├── bin/ sbin/ usr/            ← BusyBox applets
├── dev/                       ← console, null, zero, random
├── proc/                      ← procfs được mount khi khởi tạo
├── sys/                       ← sysfs được mount khi khởi tạo
├── tmp/                       ← tmpfs
├── etc/inittab
└── etc/init.d/rcS
```

Ví dụ nội dung `/etc/inittab`:

```ini
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
```

`/etc/init.d/rcS` (minh họa nguyên lý; **kiểm tra script có sẵn trước khi thay thế**):

```sh
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /tmp
hostname embedded-lab
printf 'Boot completed! Welcome to Embedded Linux Lab 1\n'
```

Đặt execute bit: `chmod +x rootfs/initramfs/etc/init.d/rcS`. Tạo device nodes `/dev/console`, `/dev/null` bằng `mknod` cần quyền root và đúng major/minor; không tự `mknod` trên máy Host ngoài thư mục rootfs.

### 6.3. Đóng gói initramfs

```bash
cd ~/LOS201/embedded_lab1
mkdir -p output
(
  cd rootfs/initramfs
  find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9
) > output/initramfs.cpio.gz

ls -lh output/initramfs.cpio.gz
gzip -dc output/initramfs.cpio.gz | cpio -t 2>/dev/null | head -30
```

Archive cần có `/sbin/init`, `etc/inittab`, `etc/init.d/rcS` và BusyBox executable/app symlinks. Giữ bản `rootfs/initramfs/` để sửa mà không phải dựng lại toàn bộ rootfs.

## 7. Boot trên QEMU và kiểm chứng

### 7.1. Boot trực tiếp kernel (đường chạy chính)

**HOST `$`:**

```bash
cd ~/LOS201/embedded_lab1
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 \
  -kernel output/zImage \
  -dtb output/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' \
  -nographic -no-reboot
```

**QEMU `/ #`:**

```sh
uname -a
cat /proc/cmdline
cat /proc/cpuinfo
cat /proc/meminfo
ps
mount
ls -l /dev/console /dev/null
cat /sys/devices/system/cpu/online
cat /sys/devices/system/cpu/possible
```

Các mục cần thấy: ARMv7 Linux 5.15, BusyBox shell hoạt động, `init` PID 1, `proc` và `sysfs` đã mount. Dấu `SMP` trong `uname` chứng minh kernel có hỗ trợ đa CPU, **không** tự xác nhận số CPU online.

Thoát `-nographic`: **Ctrl+A → X**.

## 8. Kết quả quan sát

- Đã tạo ảnh `zImage` và DTB ARM cho `vexpress-a9`.
- BusyBox static ARM build thành công; initramfs khoảng 1,1 MB ở phiên thực hành đầu, kích thước có thể thay đổi khi đóng gói lại.
- QEMU boot đến BusyBox shell; `uname -a` ghi `Linux 5.15.0 ... armv7l`.
- `/etc/inittab` chạy `rcS`; `proc`, `sysfs` và `tmpfs` sẵn sàng.
- Thí nghiệm U-Boot 2022.04 kiểm tra môi trường và nạp kernel qua `bootz`.
- `possible` có thể hiển thị `0-3` trong khi `online`/`/proc/cpuinfo` chỉ cho thấy 2 processor được kích hoạt; đây là các khái niệm khác nhau.

## 9. Troubleshooting

| Lỗi | Nguyên nhân có thể | Kiểm tra |
|---|---|---|
| menuconfig hiện `Linux/x86` | Chưa export `ARCH=arm` | `make ARCH=arm vexpress_defconfig` rồi mở lại menuconfig. |
| `Exec format error` | Binary Host/ARM sai kiến trúc | `file _install/bin/busybox`; cross-compile lại. |
| Kernel panic: No working init | Sai `rdinit` hoặc thiếu `/sbin/init`/dynamic libs | Liệt kê `cpio -t`; kiểm tra `file busybox`. |
| Không có console | UART/bootargs sai | `console=ttyAMA0,115200`; PL011 built-in. |
| `FDT_ERR_BADSTRUCTURE` | DTB nạp sai/chồng RAM | Kiểm tra `fdt addr`, `fdt header`, địa chỉ loader. |
| `VFS: Unable to mount root fs` | Initramfs thiếu/hỏng | Xem `-initrd`, `gzip -t`, `cpio -t`. |
| Nhìn thấy `possible=0-3` khi `-smp 2` | `possible` ≠ `online` | `cat .../online`; `grep processor /proc/cpuinfo`. |

## 10. Checklist và tài liệu

- [ ] `configs/kernel.config` lưu đúng cấu hình ARM.
- [ ] `configs/busybox.config` lưu cấu hình static.
- [ ] `output/zImage` và `output/vexpress-v2p-ca9.dtb` đúng target.
- [ ] `output/initramfs.cpio.gz` giải nén/liệt kê hợp lệ.
- [ ] QEMU boot đến `/ #` với đúng kernel, initramfs.
- [ ] Đã ghi `uname`, `ps`, `/proc/cpuinfo`, `mount` và ảnh console.
- [ ] Nếu báo cáo kiểm chứng U-Boot → Linux, có log **thực sự chạy `bootz`**.

**Tài liệu:** đề `LAB01_EmbeddedLinux.docx` của học phần; báo cáo Lab 1 của nhóm; [Linux kernel docs](https://www.kernel.org/doc/html/v5.15/) · [QEMU documentation](https://www.qemu.org/docs/master/system/arm/versatile.html) · [BusyBox](https://busybox.net/) · [U-Boot](https://docs.u-boot.org/).

README này mô tả quy trình tái lập. Các biến `KERNEL_SRC`, `BUSYBOX_SRC`, U-Boot load address và kích thước initramfs phải được xác minh theo bản source/output đang có, không suy từ máy của tác giả tham khảo.
