#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 2   // ~2 потока = ~50% одного ядра → ~12.5% от общей мощности
                        // Для увеличения нагрузки можно поднять до 6–8

void* cpu_intensive_task(void* arg) {
    double result = 0.0;
    long i = 0;

    while (1) {
        result += sin(i) * cos(i);
        i++;
        if (i % 1000000 == 0) {
            printf("Thread %lu: %.2f\n", (unsigned long)pthread_self(), result);  // Отладка
        }
    }

    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int rc;

    printf("Запуск тестовой нагрузки CPU...\n");
    printf("Нажмите Ctrl+C для остановки.\n");

    for (int t = 0; t < NUM_THREADS; t++) {
        rc = pthread_create(&threads[t], NULL, cpu_intensive_task, NULL);
        if (rc) {
            fprintf(stderr, "Ошибка создания потока (%d)\n", rc);
            exit(-1);
        }
    }

    // Работаем бесконечно, пока не нажат Ctrl+C
    pause();

    return 0;
}