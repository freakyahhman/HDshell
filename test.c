#include <stdio.h>
#include <unistd.h>

int main() {
    printf("OS Project: Chạy thành công trên Linux!\n");
    printf("Process ID hiện tại: %d\n", getpid());
    return 0;
}