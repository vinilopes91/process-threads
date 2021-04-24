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

struct shared_area
{
    sem_t mutex1; // Para sinalizar entre os processos p1, p2, p3
    sem_t mutex2; // Para regular retirada da fila entre t1 e t2 de P4
    int ready_for_produce;
    int queue_size;
    int rear;
    int front;
    int queue[QUEUESIZE];
};

struct shared_area_f2
{
    int queue_size;
    int queue[QUEUESIZE];
    int rear;
    int front;
    int thread_turn; // 1=P5, 2=P6, 3,4,5=P7
    int bigger;
    int smaller;
    int process5_count;
    int process6_count;
    int printed_numbers;
};

void *handle_thread_p4(void *ptr);
void *handle_threads_p7_t2(void *ptr);
void *handle_threads_p7_t3(void *ptr);
int rand_interval(int a, int b);
void signal_handler_consumers(int p);
void signal_handler_produtores(int p);
int moda(int frequency_array[], int size);

int ready_for_pickup = 0;
int frequency_printed_numbers[MAXNUMBER + 1];
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

    shmid_fifo1 = shmget(fifo1, sizeof(struct shared_area), 0666 | IPC_CREAT);

    if (shmid_fifo1 == -1)
    {
        printf("shmget falhou ao criar fifo1 no processo 4\n");
        exit(-1);
    }

    shared_memory_fifo1 = shmat(shmid_fifo1, (void *)0, 0);

    if (shared_memory_fifo1 == (void *)-1)
    {
        printf("shmat falhou na fifo1\n");
        exit(-1);
    }

    shared_area_ptr_fifo1 = (struct shared_area *)shared_memory_fifo1;

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

    shared_area_ptr_fifo1->queue_size = 0;
    shared_area_ptr_fifo1->front = 0;
    shared_area_ptr_fifo1->rear = 0;
    shared_area_ptr_fifo1->ready_for_produce = 1;

    /* Fim da inicialização de FIFO1 */

    shmid_fifo2 = shmget(fifo2, sizeof(struct shared_area_f2), 0666 | IPC_CREAT);

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
    shared_area_ptr_fifo2->process6_count = 0;
    shared_area_ptr_fifo2->queue_size = 0;
    shared_area_ptr_fifo2->printed_numbers = 0;
    shared_area_ptr_fifo2->thread_turn = 2;

    /* Fim da inicialização de FIFO2 */

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

    /* Fim da inicialização dos pipes */

    process4 = fork();

    if (process4 == -1)
    {
        printf("Erro ao criar o processo P4\n");
        exit(-1);
    }
    else if (process4 == 0)
    {
        signal(SIGUSR1, signal_handler_consumers);
        pthread_t thread;
        int res;

        close(pipe01[0]);
        close(pipe02[0]);

        pthread_create(&thread, NULL, handle_thread_p4, (void *)shared_area_ptr_fifo1);

        for (;;)
        {
            res = 0;
            sem_wait(&shared_area_ptr_fifo1->mutex2);
            if (ready_for_pickup == 1 && shared_area_ptr_fifo1->ready_for_produce == 0)
            {
                if (shared_area_ptr_fifo1->queue_size > 0)
                {
                    res = shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->front]; // Pega o primeiro da fila

                    shared_area_ptr_fifo1->front = (shared_area_ptr_fifo1->front + 1) % QUEUESIZE; // Aponta o front para o próximo elemento
                    shared_area_ptr_fifo1->queue_size -= 1;                                        // Diminui o tamanho atual da fila


                    if (shared_area_ptr_fifo1->queue_size == 0)
                    {
                        ready_for_pickup = 0;
                        shared_area_ptr_fifo1->ready_for_produce = 1;
                    }
                }
            }
            sem_post(&shared_area_ptr_fifo1->mutex2);
            if (res != 0)
            {
                write(pipe01[1], &res, sizeof(int));
            }
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
            close(pipe01[0]);
            close(pipe02[0]);
            close(pipe01[1]);
            close(pipe02[1]);

            for (;;)
            {
                sem_wait((sem_t *)&shared_area_ptr_fifo1->mutex1);
                if (shared_area_ptr_fifo1->queue_size < 10 && shared_area_ptr_fifo1->ready_for_produce == 1)
                {
                    int x = rand_interval(MINNUMBER, MAXNUMBER);
                    shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->rear] = x; // Adiciona na traseira da fila
                    shared_area_ptr_fifo1->queue_size += 1;
                    shared_area_ptr_fifo1->rear = (shared_area_ptr_fifo1->rear + 1) % QUEUESIZE;
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

        close(pipe02[0]);
        close(pipe01[1]);
        close(pipe02[1]);

        while (1)
        {
            if (shared_area_ptr_fifo2->thread_turn == 1)
            {
                if (shared_area_ptr_fifo2->queue_size == 10)
                {
                    shared_area_ptr_fifo2->thread_turn = rand_interval(3, 5);
                }
                else
                {
                    read(pipe01[0], &res, sizeof(int));
                    shared_area_ptr_fifo2->process5_count += 1;
                    shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->rear] = res;             // Adiciona na traseira da fila
                    shared_area_ptr_fifo2->queue_size += 1;                                      // Soma ao tamanho atual da fila
                    shared_area_ptr_fifo2->rear = (shared_area_ptr_fifo2->rear + 1) % QUEUESIZE; // Recalcula o valor da traseira
                    shared_area_ptr_fifo2->thread_turn = rand_interval(1, 5);
                }
            }
        }
        exit(0);
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

        close(pipe01[0]);
        close(pipe01[1]);
        close(pipe02[1]);

        while (1)
        {
            if (shared_area_ptr_fifo2->thread_turn == 2)
            {
                if (shared_area_ptr_fifo2->queue_size == 10)
                {
                    shared_area_ptr_fifo2->thread_turn = rand_interval(3, 5);
                }
                else
                {
                    read(pipe02[0], &res, sizeof(int));
                    shared_area_ptr_fifo2->process6_count += 1;
                    shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->rear] = res;             // Adiciona na traseira da fila
                    shared_area_ptr_fifo2->queue_size += 1;                                      // Soma ao tamanho atual da fila
                    shared_area_ptr_fifo2->rear = (shared_area_ptr_fifo2->rear + 1) % QUEUESIZE; // Recalcula o valor da traseira
                    shared_area_ptr_fifo2->thread_turn = rand_interval(1, 5);
                }
            }
        }
        exit(0);
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
        pthread_t threads[2];

        for (int i = 0; i < MAXNUMBER + 1; i++)
        {
            frequency_printed_numbers[i] = 0; // preenche com 0 o array de frequencia.
        }

        pthread_create(&threads[0], NULL, handle_threads_p7_t2, (void *)shared_area_ptr_fifo2);
        pthread_create(&threads[1], NULL, handle_threads_p7_t3, (void *)shared_area_ptr_fifo2);

        for (;;)
        {
            if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
            {
                break;
            }
            if (shared_area_ptr_fifo2->thread_turn == 3)
            {
                if (shared_area_ptr_fifo2->queue_size > 0)
                {
                    int random_number = shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->front]; // Pega o elemento no começo da fila

                    shared_area_ptr_fifo2->front = (shared_area_ptr_fifo2->front + 1) % QUEUESIZE; // Aponta para o novo primeiro elemento da fila
                    shared_area_ptr_fifo2->queue_size -= 1;                                        // Diminui um no tamanho atual da fila

                    if (shared_area_ptr_fifo2->printed_numbers == 0) // Seta o primeiro elemento como maior e menor para comparar entre os outros elementos impressos.
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

                    frequency_printed_numbers[random_number]++; // Para calculo posterior da moda.

                    printf("Numero impresso: %d\n", random_number);
                    fflush(stdout);
                    shared_area_ptr_fifo2->printed_numbers += 1;

                    if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
                    {
                        break;
                    }

                    shared_area_ptr_fifo2->thread_turn = rand_interval(1, 2);
                }
            }
        }

        /* Finalizando processos e pipes */
        kill(process5, 9);
        kill(process6, 9);
        close(pipe01[0]);
        close(pipe02[0]);
        close(pipe01[1]);
        close(pipe02[1]);
        kill(process4, 9);
        for (int i = 0; i < 3; i++)
        {
            kill(produtores[i], 9);
        }

        int result_moda = moda(frequency_printed_numbers, MAXNUMBER + 1);

        execution_time = clock() - execution_time;
        printf("\n\n==========================================\n");
        printf("Maior valor: %d\n", shared_area_ptr_fifo2->bigger);
        printf("Menor valor: %d\n", shared_area_ptr_fifo2->smaller);
        printf("Valores processados por P5: %d\n", shared_area_ptr_fifo2->process5_count);
        printf("Valores processados por P6: %d\n", shared_area_ptr_fifo2->process6_count);
        printf("Moda: %d\n", result_moda);
        printf("Tempo de execucao: %lf segundos\n", ((double)execution_time) / CLOCKS_PER_SEC);
        printf("==========================================\n");

        exit(0);
    }

    /* Aguarda o termino da execução dos 7 processos */
    for (int i = 0; i < 7; i++)
    {
        wait(NULL);
    }

    sem_destroy(&shared_area_ptr_fifo1->mutex1);
    sem_destroy(&shared_area_ptr_fifo1->mutex2);

    close(pipe01[0]);
    close(pipe02[0]);
    close(pipe01[1]);
    close(pipe02[1]);

    return 0;
}

void *handle_thread_p4(void *ptr)
{
    struct shared_area *shared_area_ptr_fifo1;
    shared_area_ptr_fifo1 = ((struct shared_area *)ptr);
    int res;

    for (;;)
    {
        res = 0;
        sem_wait(&shared_area_ptr_fifo1->mutex2);
        if (ready_for_pickup == 1 && shared_area_ptr_fifo1->ready_for_produce == 0)
        {
            if (shared_area_ptr_fifo1->queue_size > 0)
            {
                res = shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->front]; // Pega o primeiro da fila

                shared_area_ptr_fifo1->front = (shared_area_ptr_fifo1->front + 1) % QUEUESIZE; // Aponta o front para o próximo elemento
                shared_area_ptr_fifo1->queue_size -= 1;                                        // Diminui o tamanho atual da fila

                if (shared_area_ptr_fifo1->queue_size == 0)
                {
                    ready_for_pickup = 0;
                    shared_area_ptr_fifo1->ready_for_produce = 1;
                }
            }
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
        if (res != 0)
        {
            write(pipe02[1], &res, sizeof(int));
        }
    }
    pthread_exit(NULL);
}

void *handle_threads_p7_t2(void *ptr)
{
    struct shared_area_f2 *shared_area_ptr_fifo2;
    shared_area_ptr_fifo2 = ((struct shared_area_f2 *)ptr);

    for (;;)
    {
        if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
        {
            break;
        }
        if (shared_area_ptr_fifo2->thread_turn == 4)
        {
            if (shared_area_ptr_fifo2->queue_size > 0)
            {
                int random_number = shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->front]; // Pega o elemento no começo da fila

                shared_area_ptr_fifo2->front = (shared_area_ptr_fifo2->front + 1) % QUEUESIZE; // Aponta para o novo primeiro elemento da fila
                shared_area_ptr_fifo2->queue_size -= 1;                                        // Diminui um no tamanho atual da fila

                if (shared_area_ptr_fifo2->printed_numbers == 0) // Seta o primeiro elemento como maior e menor para comparar entre os outros elementos impressos.
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

                frequency_printed_numbers[random_number]++; // Para calculo posterior da moda.

                printf("Numero impresso: %d\n", random_number);
                fflush(stdout);
                shared_area_ptr_fifo2->printed_numbers += 1;

                if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
                {
                    break;
                }

                shared_area_ptr_fifo2->thread_turn = rand_interval(1, 2);
            }
        }
    }
    pthread_exit(NULL);
}

void *handle_threads_p7_t3(void *ptr)
{
    struct shared_area_f2 *shared_area_ptr_fifo2;
    shared_area_ptr_fifo2 = ((struct shared_area_f2 *)ptr);

    for (;;)
    {
        if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
        {
            break;
        }
        if (shared_area_ptr_fifo2->thread_turn == 5)
        {
            if (shared_area_ptr_fifo2->queue_size > 0)
            {
                int random_number = shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->front]; // Pega o elemento no começo da fila

                shared_area_ptr_fifo2->front = (shared_area_ptr_fifo2->front + 1) % QUEUESIZE; // Aponta para o novo primeiro elemento da fila
                shared_area_ptr_fifo2->queue_size -= 1;                                        // Diminui um no tamanho atual da fila

                if (shared_area_ptr_fifo2->printed_numbers == 0) // Seta o primeiro elemento como maior e menor para comparar entre os outros elementos impressos.
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

                frequency_printed_numbers[random_number]++; // Para calculo posterior da moda.

                printf("Numero impresso: %d\n", random_number);
                fflush(stdout);
                shared_area_ptr_fifo2->printed_numbers += 1;

                if (shared_area_ptr_fifo2->printed_numbers == ITERACTIONS)
                {
                    break;
                }

                shared_area_ptr_fifo2->thread_turn = rand_interval(1, 2);
            }
        }
    }
    pthread_exit(NULL);
}

void signal_handler_consumers(int p)
{
    ready_for_pickup = 1;
}

int rand_interval(int a, int b)
{
    return rand() % (b - a + 1) + a;
}

int moda(int frequency_array[], int size)
{
    int result = 0;

    for (int i = 0; i < size; i++)
    {
        if (frequency_array[i] > frequency_array[result])
        {
            result = i;
        }
    }

    return result;
}
