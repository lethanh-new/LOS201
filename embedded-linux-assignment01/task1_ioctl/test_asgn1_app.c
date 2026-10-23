#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE "/dev/asgn1"

static int passed = 0;
static int failed = 0;

static void check(int condition, const char *name)
{
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    if (condition)
        passed++;
    else
        failed++;
}

static void show_stats(int fd, const char *stage)
{
    struct asgn1_stats s = {0};

    if (ioctl(fd, ASGN1_GET_STATS, &s) < 0) {
        perror("GET_STATS");
        failed++;
        return;
    }

    printf("\n--- %s ---\n", stage);
    printf("open_count      = %d\n", s.open_count);
    printf("write_count     = %d\n", s.write_count);
    printf("read_count      = %d\n", s.read_count);
    printf("buffer_len      = %d\n", s.buffer_len);
    printf("last_write_size = %d\n", s.last_write_size);
    passed++;
}

static int read_fresh(char *buf, size_t size)
{
    int fd = open(DEVICE, O_RDONLY);
    ssize_t n;

    if (fd < 0) {
        perror("open for read");
        return -1;
    }

    n = read(fd, buf, size - 1);
    if (n >= 0)
        buf[n] = '\0';
    else
        perror("read");

    close(fd);
    return (int)n;
}

int main(void)
{
    int fd, mode, rc;
    char buf[128] = {0};
    char version[ASGN1_VERSION_LEN] = {0};
    struct asgn1_stats s = {0};

    printf("========== ASSIGNMENT 01: TASK 1 ==========\n");
    printf("DEVICE: %s\n", DEVICE);
    printf("RESET_IOCTL   = 0x%lx\n",
           (unsigned long)ASGN1_RESET_BUFFER);
    printf("STATS_IOCTL   = 0x%lx\n",
           (unsigned long)ASGN1_GET_STATS);
    printf("MODE_IOCTL    = 0x%lx\n",
           (unsigned long)ASGN1_SET_MODE);
    printf("VERSION_IOCTL = 0x%lx\n",
           (unsigned long)ASGN1_GET_VERSION);

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    /* Test 1: RESET */
    rc = ioctl(fd, ASGN1_RESET_BUFFER);
    check(rc == 0, "RESET_BUFFER ioctl");

    rc = ioctl(fd, ASGN1_GET_STATS, &s);
    check(rc == 0 &&
          s.read_count == 0 &&
          s.write_count == 0 &&
          s.buffer_len == 0,
          "RESET clears buffer and counters");

    memset(buf, 0, sizeof(buf));
    rc = read_fresh(buf, sizeof(buf));
    check(rc == 0, "Read after RESET returns EOF");

    /* Test 2: Echo mode */
    mode = 1;
    rc = ioctl(fd, ASGN1_SET_MODE, &mode);
    check(rc == 0, "SET_MODE = 1 (Echo)");

    rc = (int)write(fd, "hello", 5);
    check(rc == 5, "Write 5 bytes in Echo mode");

    memset(buf, 0, sizeof(buf));
    rc = read_fresh(buf, sizeof(buf));
    printf("Echo read: '%s'\n", buf);
    check(rc == 5 && strcmp(buf, "hello") == 0,
          "Echo mode preserves data");

    show_stats(fd, "STATS AFTER ECHO");

    /* Test 3: Kernel mode */
    mode = 0;
    rc = ioctl(fd, ASGN1_SET_MODE, &mode);
    check(rc == 0, "SET_MODE = 0 (Kernel)");

    rc = (int)write(fd, "world", 5);
    check(rc == 5, "Write 5 bytes in Kernel mode");

    memset(buf, 0, sizeof(buf));
    rc = read_fresh(buf, sizeof(buf));
    printf("Kernel read: '%s'\n", buf);
    check(rc == 13 &&
          strcmp(buf, "[KERNEL]world") == 0,
          "Kernel mode adds [KERNEL] prefix");

    show_stats(fd, "STATS AFTER KERNEL MODE");

    /* Test 4: Version */
    rc = ioctl(fd, ASGN1_GET_VERSION, version);
    printf("Version: %s\n", version);
    check(rc == 0 && version[0] != '\0',
          "GET_VERSION ioctl");

    /* Test 5: Invalid command */
    errno = 0;
    rc = ioctl(fd, 0xdeadbeefUL);
    check(rc == -1 && errno == ENOTTY,
          "Invalid ioctl returns ENOTTY");

    /* Test 6: Invalid mode */
    mode = 2;
    errno = 0;
    rc = ioctl(fd, ASGN1_SET_MODE, &mode);
    check(rc == -1 && errno == EINVAL,
          "Invalid mode returns EINVAL");

    /* Final RESET */
    rc = ioctl(fd, ASGN1_RESET_BUFFER);
    check(rc == 0, "Final RESET_BUFFER");

    rc = ioctl(fd, ASGN1_GET_STATS, &s);
    check(rc == 0 &&
          s.write_count == 0 &&
          s.read_count == 0 &&
          s.buffer_len == 0 &&
          s.last_write_size == 0,
          "Final counters are zero");

    memset(buf, 0, sizeof(buf));
    rc = read_fresh(buf, sizeof(buf));
    check(rc == 0, "Final buffer is empty");

    show_stats(fd, "FINAL STATS");

    close(fd);

    printf("\n========== TEST SUMMARY ==========\n");
    printf("PASSED: %d\n", passed);
    printf("FAILED: %d\n", failed);

    return failed == 0 ? 0 : 1;
}
