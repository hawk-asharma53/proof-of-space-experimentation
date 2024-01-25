#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>


int main() {
    struct sysinfo info;

    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return 1;
    }

    unsigned long long total_memory = (unsigned long long)info.totalram * info.mem_unit;
    printf("Total available memory: %llu bytes\n", total_memory);


    struct statvfs stat;

    // Specify the path to the filesystem you want to check
    const char *path = "/";

    if (statvfs(path, &stat) != 0) {
        perror("statvfs");
        return 1;
    }

    // Calculate and print the available space in bytes
    unsigned long long available_space = stat.f_bavail * stat.f_frsize;
    printf("Available space on %s: %llu bytes\n", path, available_space);

    return 0;
}