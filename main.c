#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>

#define MINNUMBER 1
#define MAXNUMBER 1000
#define ITERACTIONS 10000
#define QUEUESIZE 10
#define MEM_SZ 4096

struct shared_area
{
    sem_t mutex1; // Para os produtores escreverem
    sem_t mutex2; // Para regular retirada da fila entre t1 e t2 de P4
    int ready_for_produce;
    int queue_size;
    int queue[QUEUESIZE];
};

struct shared_area_f2
{
    int queue_size;
    int queue[QUEUESIZE];
    int process_turn;
    pthread_t threads_ids[3];
    int thread_turn;
    int bigger;
    int smaller;
    int process5_count;
    int process6_count;
    int printed_numbers;
};

void *handle_t1(void *ptr);
void *handle_t2(void *ptr);
void *handle_threads_p7(void *ptr);
int rand_interval(int a, int b);
void signal_handler_consumidores(int p);
void signal_handler_produtores(int p);
int moda(int arr[], int size);

int ready_for_pickup = 0;
int printedNumbers[ITERACTIONS];
int pipe01[2];
int pipe02[2];

int main()
{
    clock_t execution_time;
    pid_t produtores[3];
    pid_t process4;
    pid_t process5;
    pid_t process6;
    pid_t process7;

    key_t fifo1 = 5678;
    struct shared_area *shared_area_ptr_fifo1;
    void *shared_memory_fifo1 = (void *)0;
    int shmid_fifo1;

    key_t fifo2 = 4538;
    struct shared_area_f2 *shared_area_ptr_fifo2;
    void *shared_memory_fifo2 = (void *)0;
    int shmid_fifo2;

    execution_time = clock();

    if (pipe(pipe01) == -1)
    {
        printf("Erro pipe01()");
        return -1;
    }
    if (pipe(pipe02) == -1)
    {
        printf("Erro pipe02()");
        return -1;
    }

    process4 = fork();

    if (process4 == -1)
    {
        printf("Erro ao criar o processo P4\n");
        exit(-1);
    }
    else if (process4 == 0)
    {
        signal(SIGUSR1, signal_handler_consumidores);
        pthread_t threads[2];

        shmid_fifo1 = shmget(fifo1, MEM_SZ, 0666 | IPC_CREAT);

        if (shmid_fifo1 == -1)
        {
            printf("shmget falhou ao criar fifo1 no processo 4\n");
            exit(-1);
        }

        shared_memory_fifo1 = shmat(shmid_fifo1, (void *)0, 0);

        if (shared_memory_fifo1 == (void *)-1)
        {
            printf("shmat falhou nos produtores\n");
            exit(-1);
        }

        shared_area_ptr_fifo1 = (struct shared_area *)shared_memory_fifo1;

        pthread_create(&threads[0], NULL, handle_t1, (void *)shared_area_ptr_fifo1);
        pthread_create(&threads[1], NULL, handle_t2, (void *)shared_area_ptr_fifo1);

        for (int i = 0; i < 2; i++)
        {
            pthread_join(threads[i], NULL);
        }

        exit(0);
    }

    for (int i = 0; i < 3; i++)
    {
        produtores[i] = fork();

        if (produtores[i] == -1)
        {
            printf("Erro ao criar os processo P%d\n", i + 1);
            exit(-1);
        }
        if (produtores[i] == 0)
        {
            srand(time(NULL) + getpid());
            shmid_fifo1 = shmget(fifo1, MEM_SZ, 0666 | IPC_CREAT);

            if (shmid_fifo1 == -1)
            {
                printf("shmget falhou\n");
                exit(-1);
            }

            shared_memory_fifo1 = shmat(shmid_fifo1, (void *)0, 0);

            if (shared_memory_fifo1 == (void *)-1)
            {
                printf("shmat falhou nos produtores\n");
                exit(-1);
            }

            shared_area_ptr_fifo1 = (struct shared_area *)shared_memory_fifo1;

            if (i == 0)
            {
                if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex1, 1, 1) != 0)
                { // Semáforo dos processos
                    printf("sem_init falhou (FIFO 1, SEMAFORO 1)\n");
                    exit(-1);
                }
                if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex2, 0, 1) != 0)
                { // Semáforo das threads
                    printf("sem_init falhou (FIFO 1, SEMAFORO 2)\n");
                    exit(-1);
                }
                sem_wait((sem_t *)&shared_area_ptr_fifo1->mutex1);
                shared_area_ptr_fifo1->queue_size = 0;
                shared_area_ptr_fifo1->ready_for_produce = 1;
                sem_post((sem_t *)&shared_area_ptr_fifo1->mutex1);
            }

            for (;;)
            {
                if (process4 == 0) continue;
                sem_wait((sem_t *)&shared_area_ptr_fifo1->mutex1);
                if (shared_area_ptr_fifo1->queue_size < 10 && shared_area_ptr_fifo1->ready_for_produce == 1)
                {
                    int x = rand_interval(MINNUMBER, MAXNUMBER);
                    shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->queue_size] = x;
                    shared_area_ptr_fifo1->queue_size += 1;
                    if (shared_area_ptr_fifo1->queue_size == 10)
                    {
                        shared_area_ptr_fifo1->ready_for_produce = 0;
                        kill(process4, SIGUSR1);
                    }
                }
                sem_post((sem_t *)&shared_area_ptr_fifo1->mutex1);
            }
        }
    }

    process5 = fork();

    if (process5 == -1)
    {
        printf("Erro ao criar o processo P5\n");
        exit(-1);
    }
    else if (process5 == 0)
    {
        int res;
        srand(time(NULL));

        shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666 | IPC_CREAT);

        if (shmid_fifo2 == -1)
        {
            printf("shmget falhou ao criar fifo2 no processo 5\n");
            exit(-1);
        }

        shared_memory_fifo2 = shmat(shmid_fifo2, (void *)0, 0);

        if (shared_memory_fifo2 == (void *)-1)
        {
            printf("shmat falhou nos produtores\n");
            exit(-1);
        }

        shared_area_ptr_fifo2 = (struct shared_area_f2 *)shared_memory_fifo2;
        shared_area_ptr_fifo2->process5_count = 0;

        for (;;)
        {
            if (shared_area_ptr_fifo2->process_turn == 5)
            {
                if (shared_area_ptr_fifo2->queue_size == 10)
                {
                    shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                    shared_area_ptr_fifo2->process_turn = 7;
                }
                else if (shared_area_ptr_fifo2->queue_size < 10)
                {
                    read(pipe01[0], &res, sizeof(int));
                    shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
                    shared_area_ptr_fifo2->queue_size += 1;
                    shared_area_ptr_fifo2->process5_count += 1;
                    shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                    shared_area_ptr_fifo2->process_turn = 6;
                }
            }
        }
    }

    process6 = fork();

    if (process6 == -1)
    {
        printf("Erro ao criar o processo P6\n");
        exit(-1);
    }
    else if (process6 == 0)
    {
        int res;
        srand(time(NULL));

        shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666 | IPC_CREAT);

        if (shmid_fifo2 == -1)
        {
            printf("shmget falhou ao criar fifo2 no processo 6\n");
            exit(-1);
        }

        shared_memory_fifo2 = shmat(shmid_fifo2, (void *)0, 0);

        if (shared_memory_fifo2 == (void *)-1)
        {
            printf("shmat falhou nos produtores\n");
            exit(-1);
        }

        shared_area_ptr_fifo2 = (struct shared_area_f2 *)shared_memory_fifo2;
        shared_area_ptr_fifo2->process6_count = 0;

        for (;;)
        {
            if (shared_area_ptr_fifo2->process_turn == 6)
            {
                if (shared_area_ptr_fifo2->queue_size == 10)
                {
                    shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                    shared_area_ptr_fifo2->process_turn = 7;
                }
                if (shared_area_ptr_fifo2->queue_size < 10)
                {
                    read(pipe02[0], &res, sizeof(int));
                    shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
                    shared_area_ptr_fifo2->queue_size += 1;
                    shared_area_ptr_fifo2->process6_count += 1;
                    shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                    shared_area_ptr_fifo2->process_turn = 7;
                }
            }
        }
        return 0;
    }

    process7 = fork();

    if (process7 == -1)
    {
        printf("Erro ao criar P7\n");
        exit(-1);
    }
    else if (process7 == 0)
    {
        srand(time(NULL));
        pthread_t threads[3];

        shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666 | IPC_CREAT);

        if (shmid_fifo2 == -1)
        {
            printf("shmget falhou ao criar fifo2 no processo 7\n");
            exit(-1);
        }

        shared_memory_fifo2 = shmat(shmid_fifo2, (void *)0, 0);

        if (shared_memory_fifo2 == (void *)-1)
        {
            printf("shmat falhou nos produtores\n");
            exit(-1);
        }

        shared_area_ptr_fifo2 = (struct shared_area_f2 *)shared_memory_fifo2;

        shared_area_ptr_fifo2->queue_size = 0;
        shared_area_ptr_fifo2->printed_numbers = 0;
        shared_area_ptr_fifo2->process_turn = 5;

        for (int i = 0; i < 3; i++)
        {
            pthread_create(&threads[i], NULL, handle_threads_p7, (void *)shared_area_ptr_fifo2);
        }

        for (int i = 0; i < 3; i++)
        {
            shared_area_ptr_fifo2->threads_ids[i] = threads[i];
        }

        for (int i = 0; i < 3; i++)
        {
            pthread_join(threads[i], NULL);
        }

        close(pipe01[0]);
        close(pipe02[0]);
        kill(process5, 9);
        kill(process6, 9);
        kill(process4, 9);
        for (int i = 0; i < 3; i++)
        {
            kill(produtores[i], 9);
        }

        printf("\n\n==================================\n");
        printf("Maior valor: %d\n", shared_area_ptr_fifo2->bigger);
        printf("Menor valor: %d\n", shared_area_ptr_fifo2->smaller);
        printf("Moda: %d\n", moda(printedNumbers, ITERACTIONS));
        printf("Valores processados por P5: %d\n", shared_area_ptr_fifo2->process5_count);
        printf("Valores processados por P6: %d\n", shared_area_ptr_fifo2->process6_count);

        exit(0);
    }

    for (int i = 0; i < 7; i++)
    {
        wait(NULL);
    }

    execution_time = clock() - execution_time;
    printf("Tempo de execucao: %lf segundos\n", ((double)execution_time) / CLOCKS_PER_SEC);
    printf("==================================\n");

    return 0;
}

void *handle_t1(void *ptr)
{
    struct shared_area *shared_area_ptr_fifo1;
    shared_area_ptr_fifo1 = ((struct shared_area *)ptr);

    for (;;)
    {
        sem_wait(&shared_area_ptr_fifo1->mutex2);
        if (ready_for_pickup == 1)
        {
            sem_wait(&shared_area_ptr_fifo1->mutex1);
            if (shared_area_ptr_fifo1->queue_size > 0)
            {
                int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

                for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++)
                {
                    shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
                } // reorganiza a fila após a exclusão
                shared_area_ptr_fifo1->queue_size -= 1;

                write(pipe01[1], &res, sizeof(int));

                if (shared_area_ptr_fifo1->queue_size == 0)
                {
                    ready_for_pickup = 0;
                    shared_area_ptr_fifo1->ready_for_produce = 1;
                }
            }
            sem_post(&shared_area_ptr_fifo1->mutex1);
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
    }
}

void *handle_t2(void *ptr)
{
    struct shared_area *shared_area_ptr_fifo1;
    shared_area_ptr_fifo1 = ((struct shared_area *)ptr);

    for (;;)
    {
        sem_wait(&shared_area_ptr_fifo1->mutex2);
        if (ready_for_pickup == 1)
        {
            sem_wait(&shared_area_ptr_fifo1->mutex1);
            if (shared_area_ptr_fifo1->queue_size > 0)
            {
                int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

                for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++)
                {
                    shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
                } // reorganiza a fila após a exclusão
                shared_area_ptr_fifo1->queue_size -= 1;

                write(pipe02[1], &res, sizeof(int));

                if (shared_area_ptr_fifo1->queue_size == 0)
                {
                    ready_for_pickup = 0;
                    shared_area_ptr_fifo1->ready_for_produce = 1;
                }
            }
            sem_post(&shared_area_ptr_fifo1->mutex1);
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
    }
}

void *handle_threads_p7(void *ptr)
{
    struct shared_area_f2 *shared_area_ptr_fifo2;
    shared_area_ptr_fifo2 = ((struct shared_area_f2 *)ptr);

    while (shared_area_ptr_fifo2->printed_numbers < ITERACTIONS)
    {
        if (shared_area_ptr_fifo2->process_turn == 7)
        {
            if (shared_area_ptr_fifo2->queue_size > 0 && shared_area_ptr_fifo2->threads_ids[shared_area_ptr_fifo2->thread_turn] == pthread_self())
            {
                int random_number = shared_area_ptr_fifo2->queue[0];

                for (int i = 0; i < shared_area_ptr_fifo2->queue_size - 1; i++)
                {
                    shared_area_ptr_fifo2->queue[i] = shared_area_ptr_fifo2->queue[i + 1];
                }
                shared_area_ptr_fifo2->queue_size -= 1;

                if (shared_area_ptr_fifo2->printed_numbers == 0)
                {
                    shared_area_ptr_fifo2->bigger = random_number;
                    shared_area_ptr_fifo2->smaller = random_number;
                }

                if (random_number > shared_area_ptr_fifo2->bigger)
                {
                    shared_area_ptr_fifo2->bigger = random_number;
                }
                if (random_number < shared_area_ptr_fifo2->smaller)
                {
                    shared_area_ptr_fifo2->smaller = random_number;
                }

                printedNumbers[shared_area_ptr_fifo2->printed_numbers] = random_number; // Para calculo posterior da moda.

                printf("Numero impresso: %d\n", random_number);
                fflush(stdout);
                shared_area_ptr_fifo2->printed_numbers += 1;

                if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
                {
                    break;
                }

                int random_process = rand_interval(1, 10);
                if (random_process < 6)
                {
                    shared_area_ptr_fifo2->process_turn = 5;
                }
                else
                {
                    shared_area_ptr_fifo2->process_turn = 6;
                }
            }
            else if (shared_area_ptr_fifo2->queue_size == 0)
            {
                int random_process = rand_interval(1, 10);
                if (random_process < 6)
                {
                    shared_area_ptr_fifo2->process_turn = 5;
                }
                else
                {
                    shared_area_ptr_fifo2->process_turn = 6;
                }
            }
        }
    }
    pthread_exit(NULL);
}

void signal_handler_consumidores(int p)
{
    ready_for_pickup = 1;
}

int rand_interval(int a, int b)
{
    return rand() % (b - a + 1) + a;
}

int moda(int arr[], int size)
{
    int count = 1;
    int tempCount;
    int temp = 0;

    int result = arr[0]; // Começa pelo primeiro elemento

    for (int i = 0; i < (size - 1); i++)
    {
        temp = arr[i]; // Guarda o elemento que esta iterando
        tempCount = 0;
        for (int j = 1; j < size; j++) // Varre o array em busca do elemento temp
        {
            if (temp == arr[j])
            {
                tempCount++;
            }
        }
        if (tempCount > count) // Se o novo elemento tiver mais ocorrencias troca
        {
            result = temp;
            count = tempCount;
        }
    }
    return result;
}
