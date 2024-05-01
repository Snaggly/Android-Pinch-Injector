#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>

#define WRITE(fd, event) \
    ret = write(*fd, &event, sizeof(event)); \
    if (ret < sizeof(event)) { \
    fprintf(stderr, "Write event failed: %s\n", strerror(errno)); \
    close(*fd); \
    return -1; \
    }

struct motion_range {
    struct input_absinfo ABS_MT_X_TRACKING;
    struct input_absinfo ABS_MT_Y_TRACKING;
    struct input_absinfo ABS_MT_PRESSURE_TRACKING;
} motionRange;

__s32 previousX1 = 0;
__s32 previousY1 = 0;
__s32 previousX2 = 0;
__s32 previousY2 = 0;

static int determine_touch_device(int *fd) {
    uint8_t *bits = NULL;
    ssize_t bits_size = 0;
    motionRange.ABS_MT_X_TRACKING.maximum = 0;
    motionRange.ABS_MT_Y_TRACKING.maximum = 0;
    motionRange.ABS_MT_PRESSURE_TRACKING.maximum = 0;
    int j, k;
    volatile int res;
    while (1) {
        res = ioctl(*fd, EVIOCGBIT(EV_ABS, bits_size), bits);
        if (res < bits_size) {
            break;
        }
        bits_size = res + 16;
        bits = realloc(bits, bits_size * 2);
        if (bits == NULL) {
            fprintf(stderr, "failed to allocate buffer of size %d\n", (int) bits_size);
            *fd = 0;
            return 0;
        }
    }
    for (j = 0; j < res; j++) {
        for (k = 0; k < 8; k++)
            if (bits[j] & 1 << k) {
                int index = j * 8 + k;
                switch (index) {
                    case 53: //X
                        if (ioctl(*fd, EVIOCGABS(j * 8 + k), &(motionRange.ABS_MT_X_TRACKING)) !=
                            0) {
                            motionRange.ABS_MT_X_TRACKING.maximum = 0;
                        }
                        break;
                    case 54: //Y
                        if (ioctl(*fd, EVIOCGABS(j * 8 + k), &(motionRange.ABS_MT_Y_TRACKING)) !=
                            0) {
                            motionRange.ABS_MT_Y_TRACKING.maximum = 0;
                        }
                        break;
                    case 58: //Pressure
                        if (ioctl(*fd, EVIOCGABS(j * 8 + k),
                                  &(motionRange.ABS_MT_PRESSURE_TRACKING)) != 0) {
                            motionRange.ABS_MT_PRESSURE_TRACKING.maximum = 0;
                        }
                        break;
                }
            }
    }
    free(bits);
    if (motionRange.ABS_MT_X_TRACKING.maximum > 0 && motionRange.ABS_MT_Y_TRACKING.maximum > 0) {
        return 1;
    } else {
        *fd = 0;
        return 0;
    }
}

int find_input_device() {
    int fd = 0;
    for (int i = 0; i < 128; i++) {
        char fullPath[512];
        sprintf(fullPath, "/dev/input/event%d", i);
        fd = open(fullPath, O_RDWR);
        if (fd > 0 && determine_touch_device(&fd)) {
            break;
        }
    }

    return fd;
}

__s32 lerp(__s32 start, __s32 end, double alpha) {
    return (end - start) * alpha + start;
}

int write_event_down(int *fd, __s32 x, __s32 y) {
    struct input_event event;
    int ret;

    event.type = EV_ABS;
    event.code = ABS_MT_TRACKING_ID;
    event.value = 0x00;
    WRITE(fd, event)

    previousX1 = x;
    event.type = EV_ABS;
    event.code = ABS_MT_POSITION_X;
    event.value = x;
    WRITE(fd, event)
    previousY1 = y;
    event.type = EV_ABS;
    event.code = ABS_MT_POSITION_Y;
    event.value = y;
    WRITE(fd, event)

    if (motionRange.ABS_MT_PRESSURE_TRACKING.maximum > 0) {
        event.type = EV_ABS;
        event.code = ABS_MT_PRESSURE;
        event.value = motionRange.ABS_MT_PRESSURE_TRACKING.maximum;
        WRITE(fd, event)
    }

    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    WRITE(fd, event)
    return 0;
}

int write_event_move(int *fd, __s32 x, __s32 y) {
    struct input_event event;
    int ret;

    if (previousX1 != x) {
        previousX1 = x;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_X;
        event.value = x;
        WRITE(fd, event)
    }
    if (previousY1 != y) {
        previousY1 = y;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_Y;
        event.value = y;
        WRITE(fd, event)
    }

    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    WRITE(fd, event)
    return 0;
}

int write_event_up(int *fd) {
    struct input_event event;
    int ret;

    if (motionRange.ABS_MT_PRESSURE_TRACKING.maximum > 0) {
        event.type = EV_ABS;
        event.code = ABS_MT_PRESSURE;
        event.value = motionRange.ABS_MT_PRESSURE_TRACKING.minimum;
        WRITE(fd, event)
    }

    event.type = EV_ABS;
    event.code = ABS_MT_TRACKING_ID;
    event.value = -0x01;
    WRITE(fd, event)

    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    WRITE(fd, event)
    return 0;
}

int main(int argc, char *argv[]) {
    int fd;
    char *endptr;
    int ret;

    if (argc != 6) {
        fprintf(stderr,
                "Usage: %s startX startY endX endY duration\n\n\nstartX,startY,endX,endY: Relative in %% from center\nduration: How long pinching takes in milliseconds\n\n",
                argv[0]);
        return 1;
    }

    fd = find_input_device();
    if (fd < 0) {
        fprintf(stderr, "Could not open touch controller: %s\n", strerror(errno));
        return 1;
    } else if (!fd) {
        printf("Could not find touch device\n");
        return 1;
    }


    int from_x = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'from_x'\n");
        return 1;
    }
    int from_y = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'from_y'\n");
        return 1;
    }
    int to_x = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'to_x'\n");
        return 1;
    }
    int to_y = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'to_y'\n");
        return 1;
    }
    long duration = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'to_y'\n");
        return 1;
    }
    if (from_x == to_x && from_y == to_y) {
        printf("From and To are the same!\n");
        return 1;
    }

    __s32 rangeX = (motionRange.ABS_MT_X_TRACKING.maximum - motionRange.ABS_MT_X_TRACKING.minimum);
    __s32 rangeY = (motionRange.ABS_MT_Y_TRACKING.maximum - motionRange.ABS_MT_Y_TRACKING.minimum);
    __s32 startPointX = (rangeX * from_x / 100.0) + motionRange.ABS_MT_X_TRACKING.minimum;
    __s32 startPointY = (rangeY * from_y / 100.0) + motionRange.ABS_MT_Y_TRACKING.minimum;
    __s32 endPointX = (rangeX * to_x / 100.0) + motionRange.ABS_MT_X_TRACKING.minimum;
    __s32 endPointY = (rangeX * to_y / 100.0) + motionRange.ABS_MT_X_TRACKING.minimum;

    ret = write_event_down(&fd, startPointX, startPointY);
    if (ret < 0) {
        return 1;
    }

    if (duration > 400) {
        clock_t down, end, when;
        double elapsed, alpha;
        down = clock();
        end = ((duration * CLOCKS_PER_SEC) / 1000.0) + down;
        when = clock();
        while (when < end) {
            elapsed = (double) (when - down) * 1000.0 / CLOCKS_PER_SEC;
            alpha = elapsed / duration;
            __s32 pointX = lerp(startPointX, endPointX, alpha);
            __s32 pointY = lerp(startPointY, endPointY, alpha);
            ret = write_event_move(&fd, pointX, pointY);
            if (ret < 0) {
                return 1;
            }
            when = clock();
        }
    } else {
        double alpha;
        for (int step = 1; step <= duration; step++) {
            alpha = step / duration;
            __s32 pointX = lerp(startPointX, endPointX, alpha);
            __s32 pointY = lerp(startPointY, endPointY, alpha);
            ret = write_event_move(&fd, pointX, pointY);
            if (ret < 0) {
                return 1;
            }
            usleep(1000);
        }
    }

    ret = write_event_up(&fd);
    if (ret < 0) {
        return 1;
    }

    close(fd);
    return 0;
}