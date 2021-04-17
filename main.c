#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
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
    pid_t paused_process;
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

int ready_for_pickup = 0;
int printedNumbers = 0;
int pipe01[2];
int pipe02[2];

int main()
{
    pid_t produtores[3];
    pid_t process4;
    pid_t process5;
    pid_t process6;
    pid_t process7;
    pid_t main_process = getpid();

    key_t fifo1 = 5678;
    struct shared_area *shared_area_ptr_fifo1;
    void *shared_memory_fifo1 = (void *)0;
    int shmid_fifo1;

    key_t fifo2 = 4538;
    struct shared_area_f2 *shared_area_ptr_fifo2;
    void *shared_memory_fifo2 = (void *)0;
    int shmid_fifo2;

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

        if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex2, 0, 1) != 0)
        {
            printf("sem_init falhou (FIFO 1, SEMAFORO 2)\n");
            exit(-1);
        }

        if (ready_for_pickup != 1)
        {
            pause();
        }

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
            signal(SIGUSR2, signal_handler_produtores);
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
                shared_area_ptr_fifo1->queue_size = 0;
                if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex1, 1, 1) != 0)
                { // Cria um semáforo pra ser utilizado entre processos (p1, p2, p3) (1)
                    printf("sem_init falhou (FIFO 1, SEMAFORO 1)\n");
                    exit(-1);
                }
            }

            for (;;)
            {
                sem_wait((sem_t *)&shared_area_ptr_fifo1->mutex1);
                if (shared_area_ptr_fifo1->queue_size < 10)
                {
                    int x = rand_interval(MINNUMBER, MAXNUMBER);
                    shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->queue_size] = x; // Insere na última posição da fila
                    shared_area_ptr_fifo1->queue_size += 1;                              // Soma um no tamanho atual

                    if (shared_area_ptr_fifo1->queue_size == 10)
                    {
                        shared_area_ptr_fifo1->paused_process = getpid();
                        kill(process4, SIGUSR1);
                        pause();
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
            printf("shmget falhou ao criar fifo1 no processo 4\n");
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
            if (shared_area_ptr_fifo2->queue_size == 10 && shared_area_ptr_fifo2->process_turn == 5)
            {
                shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                printf("Fila cheia mandou para P7 e altrou a thread para: %d\n\n", shared_area_ptr_fifo2->thread_turn);
                shared_area_ptr_fifo2->process_turn = 7;
            }
            else if (shared_area_ptr_fifo2->queue_size < 10 && shared_area_ptr_fifo2->process_turn == 5)
            {
                read(pipe01[0], &res, sizeof(int));
                shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
                shared_area_ptr_fifo2->queue_size += 1;
                shared_area_ptr_fifo2->process5_count += 1;
                printf("Novo queue size: %d\n", shared_area_ptr_fifo2->queue_size);
                shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                printf("thread turn: %d\n", shared_area_ptr_fifo2->thread_turn);
                printf("process turn: 6\n\n");
                shared_area_ptr_fifo2->process_turn = 6;
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
            printf("shmget falhou ao criar fifo1 no processo 4\n");
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
            if (shared_area_ptr_fifo2->queue_size == 10 && shared_area_ptr_fifo2->process_turn == 6)
            {
                shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                printf("Fila cheia mandou para P7 e altrou a thread para: %d\n\n", shared_area_ptr_fifo2->thread_turn);
                shared_area_ptr_fifo2->process_turn = 7;
            }
            if (shared_area_ptr_fifo2->queue_size < 10 && shared_area_ptr_fifo2->process_turn == 6)
            {
                read(pipe02[0], &res, sizeof(int));
                shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
                shared_area_ptr_fifo2->queue_size += 1;
                shared_area_ptr_fifo2->process6_count += 1;
                printf("Novo queue size: %d\n", shared_area_ptr_fifo2->queue_size);
                shared_area_ptr_fifo2->thread_turn = rand_interval(0, 2);
                printf("thread turn: %d\n", shared_area_ptr_fifo2->thread_turn);
                printf("process turn: 7\n\n");
                shared_area_ptr_fifo2->process_turn = 7;
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
        int random_number;
        pthread_t threads[3];

        shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666 | IPC_CREAT);

        if (shmid_fifo2 == -1)
        {
            printf("shmget falhou ao criar fifo1 no processo 4\n");
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

        printf("Maior valor: %d\n", shared_area_ptr_fifo2->bigger);
        printf("Menor valor: %d\n", shared_area_ptr_fifo2->smaller);
        printf("Valores processados por P5: %d\n", shared_area_ptr_fifo2->process5_count);
        printf("Valores processados por P6: %d\n", shared_area_ptr_fifo2->process6_count);

        exit(0);
    }

    waitpid(process7, NULL, WUNTRACED);

    if (getpid() == main_process)
    {
        close(pipe01[0]);
        close(pipe02[0]);
        kill(process4, 9);
        kill(process5, 9);
        kill(process6, 9);
        for (int i = 0; i < 3; i++)
        {
            kill(produtores[i], 9);
        }
    }

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
                    kill(shared_area_ptr_fifo1->paused_process, SIGUSR2);
                }
            }
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
    }
    printf("Finalizou T1\n");
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
                    kill(shared_area_ptr_fifo1->paused_process, SIGUSR2);
                }
            }
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
    }
    printf("Finalizou T2\n");
}

void *handle_threads_p7(void *ptr)
{
    struct shared_area_f2 *shared_area_ptr_fifo2;
    shared_area_ptr_fifo2 = ((struct shared_area_f2 *)ptr);

    while (shared_area_ptr_fifo2->printed_numbers < ITERACTIONS)
    {
        if (shared_area_ptr_fifo2->queue_size > 0 && shared_area_ptr_fifo2->process_turn == 7 && shared_area_ptr_fifo2->threads_ids[shared_area_ptr_fifo2->thread_turn] == pthread_self())
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

            shared_area_ptr_fifo2->printed_numbers += 1;

            printf("Numero impresso: %d\n", shared_area_ptr_fifo2->queue[0]);
            int random_process = rand_interval(1, 10);
            if (random_process < 6)
            {
                printf("Novo queue size: %d\n", shared_area_ptr_fifo2->queue_size);
                printf("thread mandou para processo: 5\n\n");
                shared_area_ptr_fifo2->process_turn = 5;
            }
            else
            {
                printf("Novo queue size: %d\n", shared_area_ptr_fifo2->queue_size);
                printf("thread mandou para processo: 6\n\n");
                shared_area_ptr_fifo2->process_turn = 6;
            }
        }
        else if (shared_area_ptr_fifo2->queue_size == 0 && shared_area_ptr_fifo2->process_turn == 7)
        {
            int random_process = rand_interval(1, 10);
            if (random_process < 6)
            {
                printf("Fila vazia, thread redirecionado para processo: 5\n\n");
                shared_area_ptr_fifo2->process_turn = 5;
            }
            else
            {
                printf("Fila vazia, thread redirecionado para processo: 6\n\n");
                shared_area_ptr_fifo2->process_turn = 6;
            }
        }
    }
    pthread_exit(NULL);
}

void signal_handler_consumidores(int p)
{
    ready_for_pickup = 1;
}

void signal_handler_produtores(int p) {}

int rand_interval(int a, int b)
{
    return rand() % (b - a + 1) + a;
}
