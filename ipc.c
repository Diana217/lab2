#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>

// Константи
#define SHM_NAME "/my_shared_memory"
#define MMAP_FILE "/tmp/mmap_test_file"
#define FILE_IPC_PATH "/tmp/ipc_file_test"
#define FIFO_PATH "/tmp/my_fifo"
#define MESSAGE_SIZE (1024 * 1024) // 1MB
#define ITERATIONS 100
#define SEM_NAME "/my_semaphore"

// Функція для вимірювання часу
double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + 
           (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Структура для зберігання результатів тестування
typedef struct {
    char method_name[50];
    double total_time;       // Загальний час (секунди)
    double avg_latency;      // Середня затримка на ітерацію (секунди)
    double throughput;       // Пропускна здатність (байт/секунду)
    size_t channel_capacity; // Ємність каналу (байти)
} IPC_Result;

// Shared Memory IPC
IPC_Result test_shared_memory() {
    IPC_Result result;
    strcpy(result.method_name, "Shared Memory");
    result.channel_capacity = MESSAGE_SIZE;

    printf("Тестування Shared Memory IPC...\n");
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, MESSAGE_SIZE) == -1) {
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    char *ptr = mmap(NULL, MESSAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    struct timespec start, end;
    if (pid == 0) {
        for (int i = 0; i < ITERATIONS; i++) {
            volatile char tmp = ptr[0];
        }
        munmap(ptr, MESSAGE_SIZE);
        exit(EXIT_SUCCESS);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERATIONS; i++) {
            memset(ptr, 'A', MESSAGE_SIZE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        wait(NULL);
        double time_taken = get_time_diff(start, end);
        result.total_time = time_taken;
        result.avg_latency = time_taken / ITERATIONS;
        result.throughput = (double)(MESSAGE_SIZE * ITERATIONS) / time_taken;
        printf("Shared Memory: Загальний час: %.6f секунд\n", result.total_time);
        printf("Shared Memory: Середній час на ітерацію: %.6f секунд\n", result.avg_latency);
        printf("Shared Memory: Пропускна здатність: %.2f байт/секунду\n", result.throughput);
        munmap(ptr, MESSAGE_SIZE);
        shm_unlink(SHM_NAME);
    }
    return result;
}

// mmap (Анонімне відображення) IPC
IPC_Result test_mmap_anonymous() {
    IPC_Result result;
    strcpy(result.method_name, "mmap (Анонімне)");
    result.channel_capacity = MESSAGE_SIZE;

    printf("Тестування mmap (Анонімне відображення) IPC...\n");
    // Анонімне відображення
    char *ptr = mmap(NULL, MESSAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    struct timespec start, end;
    if (pid == 0) {
        for (int i = 0; i < ITERATIONS; i++) {
            volatile char tmp = ptr[0];
        }
        munmap(ptr, MESSAGE_SIZE);
        exit(EXIT_SUCCESS);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERATIONS; i++) {
            memset(ptr, 'A', MESSAGE_SIZE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        wait(NULL);
        double time_taken = get_time_diff(start, end);
        result.total_time = time_taken;
        result.avg_latency = time_taken / ITERATIONS;
        result.throughput = (double)(MESSAGE_SIZE * ITERATIONS) / time_taken;
        printf("mmap (Анонімне): Загальний час: %.6f секунд\n", result.total_time);
        printf("mmap (Анонімне): Середній час на ітерацію: %.6f секунд\n", result.avg_latency);
        printf("mmap (Анонімне): Пропускна здатність: %.2f байт/секунду\n", result.throughput);
        munmap(ptr, MESSAGE_SIZE);
    }
    return result;
}

// mmap (Файлове відображення) IPC
IPC_Result test_mmap_file_backed() {
    IPC_Result result;
    strcpy(result.method_name, "mmap (Файлове)");
    result.channel_capacity = MESSAGE_SIZE;

    printf("Тестування mmap (Файлове відображення) IPC...\n");
    // Створення або відкриття файлу
    int fd = open(MMAP_FILE, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    // Встановлення розміру файлу
    if (ftruncate(fd, MESSAGE_SIZE) == -1) {
        perror("ftruncate");
        close(fd);
        unlink(MMAP_FILE);
        exit(EXIT_FAILURE);
    }
    // Відображення файлу у пам'ять
    char *ptr = mmap(NULL, MESSAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        unlink(MMAP_FILE);
        exit(EXIT_FAILURE);
    }
    close(fd); // Файл більше не потрібен після відображення

    pid_t pid = fork();
    struct timespec start, end;
    if (pid == 0) {
        for (int i = 0; i < ITERATIONS; i++) {
            volatile char tmp = ptr[0];
        }
        munmap(ptr, MESSAGE_SIZE);
        exit(EXIT_SUCCESS);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERATIONS; i++) {
            memset(ptr, 'A', MESSAGE_SIZE);
        }
        // Важливо синхронізувати дані на диск
        if (msync(ptr, MESSAGE_SIZE, MS_SYNC) == -1) {
            perror("msync");
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        wait(NULL);
        double time_taken = get_time_diff(start, end);
        result.total_time = time_taken;
        result.avg_latency = time_taken / ITERATIONS;
        result.throughput = (double)(MESSAGE_SIZE * ITERATIONS) / time_taken;
        printf("mmap (Файлове): Загальний час: %.6f секунд\n", result.total_time);
        printf("mmap (Файлове): Середній час на ітерацію: %.6f секунд\n", result.avg_latency);
        printf("mmap (Файлове): Пропускна здатність: %.2f байт/секунду\n", result.throughput);
        munmap(ptr, MESSAGE_SIZE);
        unlink(MMAP_FILE);
    }
    return result;
}

// File Open / Read / Write IPC
IPC_Result test_file_rw() {
    IPC_Result result;
    strcpy(result.method_name, "File Open/Read/Write");
    result.channel_capacity = MESSAGE_SIZE;

    printf("Тестування File Open/Read/Write IPC...\n");
    // Створення або відкриття файлу
    int fd = open(FILE_IPC_PATH, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    struct timespec start, end;
    if (pid == 0) {
        char buffer[MESSAGE_SIZE];
        for (int i = 0; i < ITERATIONS; i++) {
            lseek(fd, 0, SEEK_SET);
            read(fd, buffer, MESSAGE_SIZE);
        }
        close(fd);
        exit(EXIT_SUCCESS);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERATIONS; i++) {
            write(fd, "A", MESSAGE_SIZE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        wait(NULL);
        double time_taken = get_time_diff(start, end);
        result.total_time = time_taken;
        result.avg_latency = time_taken / ITERATIONS;
        result.throughput = (double)(MESSAGE_SIZE * ITERATIONS) / time_taken;
        printf("File Open/Read/Write: Загальний час: %.6f секунд\n", result.total_time);
        printf("File Open/Read/Write: Середній час на ітерацію: %.6f секунд\n", result.avg_latency);
        printf("File Open/Read/Write: Пропускна здатність: %.2f байт/секунду\n", result.throughput);
        close(fd);
        unlink(FILE_IPC_PATH);
    }
    return result;
}

// FIFO (іменовані канали) IPC
IPC_Result test_fifo() {
    IPC_Result result;
    strcpy(result.method_name, "FIFO (іменовані канали)");
    result.channel_capacity = MESSAGE_SIZE;

    printf("Тестування FIFO IPC...\n");
    // Створення іменованого каналу
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    struct timespec start, end;
    if (pid == 0) {
        int fd = open(FIFO_PATH, O_RDONLY);
        char buffer[MESSAGE_SIZE];
        for (int i = 0; i < ITERATIONS; i++) {
            read(fd, buffer, MESSAGE_SIZE);
        }
        close(fd);
        exit(EXIT_SUCCESS);
    } else {
        int fd = open(FIFO_PATH, O_WRONLY);
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERATIONS; i++) {
            write(fd, "A", MESSAGE_SIZE);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        close(fd);
        wait(NULL);
        double time_taken = get_time_diff(start, end);
        result.total_time = time_taken;
        result.avg_latency = time_taken / ITERATIONS;
        result.throughput = (double)(MESSAGE_SIZE * ITERATIONS) / time_taken;
        printf("FIFO: Загальний час: %.6f секунд\n", result.total_time);
        printf("FIFO: Середній час на ітерацію: %.6f секунд\n", result.avg_latency);
        printf("FIFO: Пропускна здатність: %.2f байт/секунду\n", result.throughput);
        unlink(FIFO_PATH);
    }
    return result;
}

int main() {
    test_shared_memory();
    printf("\n");
    test_mmap_anonymous();
    printf("\n");
    test_mmap_file_backed();
    printf("\n");
    test_file_rw();
    printf("\n");
    test_fifo();
    printf("\n");

    return 0;
}