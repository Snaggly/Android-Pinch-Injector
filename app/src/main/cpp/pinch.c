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

#define WRITE(fd,event) \
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

/*
    0003 002f 00000000	EV_ABS       ABS_MT_SLOT          00000000
    0003 0039 00000000	EV_ABS       ABS_MT_TRACKING_ID   00000000
    0003 003a 00000400	EV_ABS       ABS_MT_PRESSURE      00000400
    0003 0035 00003b32	EV_ABS       ABS_MT_POSITION_X    000036aa
    0003 0036 00004416	EV_ABS       ABS_MT_POSITION_Y    00004554
    0003 002f 00000001	EV_ABS       ABS_MT_SLOT          00000001
    0003 0039 00000001	EV_ABS       ABS_MT_TRACKING_ID   00000001
    0003 003a 00000400	EV_ABS       ABS_MT_PRESSURE      00000400
    0003 0035 000044bb	EV_ABS       ABS_MT_POSITION_X    00004943
    0003 0036 00003bbb	EV_ABS       ABS_MT_POSITION_Y    00003a7c
    0000 0000 00000000	EV_SYN       SYN_REPORT           00000000
 */
int write_event_down(int *fd, __s32 x1, __s32 y1, __s32 x2, __s32 y2) {
    struct input_event event;
    int ret;

    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x00;
    WRITE(fd, event)
    event.type = EV_ABS;
    event.code = ABS_MT_TRACKING_ID;
    event.value = 0x00;
    WRITE(fd, event)

    if (motionRange.ABS_MT_PRESSURE_TRACKING.maximum > 0) {
        event.type = EV_ABS;
        event.code = ABS_MT_PRESSURE;
        event.value = motionRange.ABS_MT_PRESSURE_TRACKING.maximum;
        WRITE(fd, event)
    }

    previousX1 = x1;
    event.type = EV_ABS;
    event.code = ABS_MT_POSITION_X;
    event.value = x1;
    WRITE(fd, event)
    previousY1 = y1;
    event.type = EV_ABS;
    event.code = ABS_MT_POSITION_Y;
    event.value = y1;
    WRITE(fd, event)
    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x01;
    WRITE(fd, event)
    event.type = EV_ABS;
    event.code = ABS_MT_TRACKING_ID;
    event.value = 0x01;
    WRITE(fd, event)

    if (motionRange.ABS_MT_PRESSURE_TRACKING.maximum > 0) {
        event.type = EV_ABS;
        event.code = ABS_MT_PRESSURE;
        event.value = motionRange.ABS_MT_PRESSURE_TRACKING.maximum;
        WRITE(fd, event)
    }

    if (previousX2 != x2) {
        previousX2 = x2;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_X;
        event.value = x2;
        WRITE(fd, event)
    }
    if (previousY2 != y2) {
        previousY2 = y2;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_Y;
        event.value = y2;
        WRITE(fd, event)
    }
    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    WRITE(fd, event)
    return 0;
}

/*
    0003 002f 00000000	EV_ABS       ABS_MT_SLOT          00000000
    0003 0035 00003b21	EV_ABS       ABS_MT_POSITION_X    00003699
    0003 002f 00000001	EV_ABS       ABS_MT_SLOT          00000001
    0003 0035 000044cc	EV_ABS       ABS_MT_POSITION_X    00004954
    0000 0000 00000000	EV_SYN       SYN_REPORT           00000000
 */
int write_event_move(int *fd, __s32 x1, __s32 y1, __s32 x2, __s32 y2) {
    struct input_event event;
    int ret;

    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x00;
    WRITE(fd, event)

    if (previousX1 != x1) {
        previousX1 = x1;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_X;
        event.value = x1;
        WRITE(fd, event)
    }
    if (previousY1 != y1) {
        previousY1 = y1;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_Y;
        event.value = y1;
        WRITE(fd, event)
    }
    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x01;
    WRITE(fd, event)

    if (previousX2 != x2) {
        previousX2 = x2;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_X;
        event.value = x2;
        WRITE(fd, event)
    }
    if (previousY2 != y2) {
        previousY2 = y2;
        event.type = EV_ABS;
        event.code = ABS_MT_POSITION_Y;
        event.value = y2;
        WRITE(fd, event)
    }
    event.type = EV_SYN;
    event.code = SYN_REPORT;
    event.value = 0;
    WRITE(fd, event)
    return 0;
}

/*
    0003 002f 00000000	EV_ABS       ABS_MT_SLOT          00000000
    0003 003a 00000000	EV_ABS       ABS_MT_PRESSURE      00000000
    0003 0039 ffffffff	EV_ABS       ABS_MT_TRACKING_ID   ffffffff
    0003 002f 00000001	EV_ABS       ABS_MT_SLOT          00000001
    0003 003a 00000000	EV_ABS       ABS_MT_PRESSURE      00000000
    0003 0039 ffffffff	EV_ABS       ABS_MT_TRACKING_ID   ffffffff
    0000 0000 00000000	EV_SYN       SYN_REPORT           00000000
*/
int write_event_up(int *fd) {
    struct input_event event;
    int ret;

    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x00;
    WRITE(fd, event)

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
    event.type = EV_ABS;
    event.code = ABS_MT_SLOT;
    event.value = 0x01;
    WRITE(fd, event)

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

__s32 lerp(__s32 start, __s32 end, double alpha) {
    return (end - start) * alpha + start;
}

int main(int argc, char *argv[]) {
    int fd;
    char *endptr;
    int ret;

    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s from to angle duration\n\n\nfrom,to: Relative in %% from center\nangle: Degree from 0° to 90°\nduration: How long pinching takes in milliseconds\n\n",
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


    int from = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'from'\n");
        return 1;
    }
    int to = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'to'\n");
        return 1;
    }
    int angle = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'angle'\n");
        return 1;
    }
    long duration = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0') {
        printf("Could not interpret parameter: 'duration'\n");
        return 1;
    }
    if (from == to) {
        printf("From and To are the same!\n");
        return 1;
    }

    double xShift = cos(angle * (M_PI / 180));
    double yShift = sin(angle * (M_PI / 180));

    __s32 halfSizeX = (motionRange.ABS_MT_X_TRACKING.maximum
                       - motionRange.ABS_MT_X_TRACKING.minimum) / 2;
    __s32 halfSizeY = (motionRange.ABS_MT_Y_TRACKING.maximum
                       - motionRange.ABS_MT_Y_TRACKING.minimum) / 2;
    __s32 midpointX = halfSizeX + motionRange.ABS_MT_X_TRACKING.minimum;
    __s32 midpointY = halfSizeY + motionRange.ABS_MT_Y_TRACKING.minimum;
    __s32 startPointX = midpointX + (halfSizeX * (from / 100.0) * xShift);
    __s32 startPointY = midpointY + (halfSizeY * (from / 100.0) * yShift);
    __s32 startPointX2 = midpointX - (halfSizeX * (from / 100.0) * xShift);
    __s32 startPointY2 = midpointY - (halfSizeY * (from / 100.0) * yShift);
    __s32 endPointX = midpointX + (halfSizeX * (to / 100.0) * xShift);
    __s32 endPointY = midpointY + (halfSizeY * (to / 100.0) * yShift);
    __s32 endPointX2 = midpointX - (halfSizeX * (to / 100.0) * xShift);
    __s32 endPointY2 = midpointY - (halfSizeY * (to / 100.0) * yShift);

    //Start - Fingers Down
    ret = write_event_down(&fd, startPointX, startPointY, startPointX2, startPointY2);
    if (ret < 0) {
        return 1;
    }

    //Move - Fingers Move
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
        __s32 pointX2 = lerp(startPointX2, endPointX2, alpha);
        __s32 pointY2 = lerp(startPointY2, endPointY2, alpha);
        ret = write_event_move(&fd, pointX, pointY, pointX2, pointY2);
        if (ret < 0) {
            return 1;
        }
        when = clock();
    }

    //End - Fingers Up
    ret = write_event_up(&fd);
    if (ret < 0) {
        return 1;
    }

    close(fd);
    return 0;
}