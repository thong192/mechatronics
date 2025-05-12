#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

// Định nghĩa ioctl
#define DEVICE_PATH "/dev/bmp180"
#define BMP180_IOCTL_MAGIC 'b'
#define BMP180_IOCTL_READ_TEMP     _IOR(BMP180_IOCTL_MAGIC, 1, int)
#define BMP180_IOCTL_READ_PRESSURE _IOR(BMP180_IOCTL_MAGIC, 2, int)

int main() {
    int fd;
    int temp, pressure;

    // Mở device file
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Khong mo dc thiet bi /dev/bmp180");
        return errno;
    }

    // Đọc nhiệt dộ
    if (ioctl(fd, BMP180_IOCTL_READ_TEMP, &temp) < 0) {
        perror("Loi ioctl đoc nhiet đo");
        close(fd);
        return errno;
    }

    // Đọc áp suât
    if (ioctl(fd, BMP180_IOCTL_READ_PRESSURE, &pressure) < 0) {
        perror("Loi ioctl doc ap suat");
        close(fd);
        return errno;
    }

    // In kết quả ra màn hình
    printf("Nhiệt độ: %d.%d °C\n", temp / 10, abs(temp % 10));
    printf("Áp suất:  %d.%02d hPa\n", pressure / 100, pressure % 100);

    // Đóng thiết bị
    close(fd);
    return 0;
}
