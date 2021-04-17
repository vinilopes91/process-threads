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
    int process5_count;
    int process6_count;
    int printed_numbers;
};

void *handle_t1(void *ptr);
void *handle_t2(void *ptr);
int rand_interval(int a, int b);
void signalHandler(int p);
void signalHandler2(int p);

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
        signal(SIGUSR1, signalHandler);
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

        while (ready_for_pickup != 1)
            ;

        pthread_create(&threads[0], NULL, handle_t1, (void *)shared_area_ptr_fifo1);
        pthread_create(&threads[1], NULL, handle_t2, (void *)shared_area_ptr_fifo1);

        for (;;)
            ;
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
            signal(SIGUSR2, signalHandler2);
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

                    printf("Número gerado pelo processo %d: %d\n", getpid(), x);
                    printf("Tamanho da fila: %d\n", shared_area_ptr_fifo1->queue_size);

                    if (shared_area_ptr_fifo1->queue_size == 10)
                    {
                        printf("Fila encheu. %d\n", getpid());
                        printf("Pausando processo: %d\n", getpid());
                        shared_area_ptr_fifo1->paused_process = getpid();
                        kill(process4, SIGUSR1);
                        pause();
                    }
                }
                sem_post((sem_t *)&shared_area_ptr_fifo1->mutex1);
            }
        }
    }

    // process5 = fork();

    // if (process5 == -1) {
    //   printf("Erro ao criar o processo P5\n");
    //   exit(-1);
    // } else if (process5 == 0) {
    //   int res;

    //   shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666|IPC_CREAT);

    //   if (shmid_fifo2 == -1) {
    //     printf("shmget falhou ao criar fifo1 no processo 4\n");
    //     exit(-1);
    //   }

    //   shared_memory_fifo2 = shmat(shmid_fifo2, (void*)0, 0);

    //   if (shared_memory_fifo2 == (void *) -1) {
    //     printf("shmat falhou nos produtores\n");
    //     exit(-1);
    //   }

    //   shared_area_ptr_fifo2 = (struct shared_area_f2 *) shared_memory_fifo2;
    //   shared_area_ptr_fifo2->queue_size = 0;
    //   shared_area_ptr_fifo2->process_turn = 5;
    //   shared_area_ptr_fifo2->process5_count = 0;

    //   while(1) {
    //     for(;;) {
    //       if (shared_area_ptr_fifo2->queue_size <  10 && shared_area_ptr_fifo2->process_turn == 5) {
    //         read(pipe01[0], &res, sizeof(int));
    //         shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
    //         shared_area_ptr_fifo2->queue_size += 1;
    //         shared_area_ptr_fifo2->process5_count += 1;
    //         shared_area_ptr_fifo2->process_turn = 6;
    //         printf("P5 adicionou a F2\n");
    //       }
    //     }
    //   }
    // }

    // process6 = fork();

    // if (process6 == -1) {
    //   printf("Erro ao criar o processo P6\n");
    //   exit(-1);
    // } else if (process6 == 0) {
    //   int res;

    //   shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666|IPC_CREAT);

    //   if (shmid_fifo2 == -1) {
    //     printf("shmget falhou ao criar fifo1 no processo 4\n");
    //     exit(-1);
    //   }

    //   shared_memory_fifo2 = shmat(shmid_fifo2, (void*)0, 0);

    //   if (shared_memory_fifo2 == (void *) -1) {
    //     printf("shmat falhou nos produtores\n");
    //     exit(-1);
    //   }

    //   shared_area_ptr_fifo2 = (struct shared_area_f2 *) shared_memory_fifo2;
    //   shared_area_ptr_fifo2->process6_count = 0;

    //   while(1) {
    //     for(;;) {
    //       if (shared_area_ptr_fifo2->queue_size <  10 && shared_area_ptr_fifo2->process_turn == 6) {
    //         read(pipe02[0], &res, sizeof(int));
    //         shared_area_ptr_fifo2->queue[shared_area_ptr_fifo2->queue_size] = res;
    //         shared_area_ptr_fifo2->queue_size += 1;
    //         shared_area_ptr_fifo2->process6_count += 1;
    //         shared_area_ptr_fifo2->process_turn = 7;
    //         printf("P6 adicionou a F2\n");
    //       }
    //     }
    //   }
    //   return 0;
    // }

    // process7 = fork();

    // if (process7 == -1) {
    //   printf("Erro ao criar P7\n");
    //   exit(-1);
    // } else if (process7 == 0) {
    //   int random_number;
    //   pthread_t threads[3];

    //   shmid_fifo2 = shmget(fifo2, MEM_SZ, 0666|IPC_CREAT);

    //   if (shmid_fifo2 == -1) {
    //     printf("shmget falhou ao criar fifo1 no processo 4\n");
    //     exit(-1);
    //   }

    //   shared_memory_fifo2 = shmat(shmid_fifo2, (void*)0, 0);

    //   if (shared_memory_fifo2 == (void *) -1) {
    //     printf("shmat falhou nos produtores\n");
    //     exit(-1);
    //   }

    //   shared_area_ptr_fifo2 = (struct shared_area_f2 *) shared_memory_fifo2;

    //   shared_area_ptr_fifo2->printed_numbers = 0;

    //   // Criar as 3 threads

    //   while(shared_area_ptr_fifo2->printed_numbers < ITERACTIONS) {
    //     if (shared_area_ptr_fifo2->queue_size > 0 && shared_area_ptr_fifo2->process_turn == 7) {
    //       random_number = shared_area_ptr_fifo2->queue[0];
    //       for (int i = 0; i < shared_area_ptr_fifo2->queue_size - 1; i++) {
    //         shared_area_ptr_fifo2->queue[i] = shared_area_ptr_fifo2->queue[i + 1];
    //       }
    //       shared_area_ptr_fifo2->queue_size -= 1;
    //       shared_area_ptr_fifo2->process_turn = 5;
    //       printf("Numero aleatório gerado: %d\n", random_number);
    //     }
    //   }
    //   exit(0);
    // }

    // int z;
    // wait(&z); // Esperar pelo p7 e finalizar o resto dos processos

    if (getpid() == main_process)
    {
        sleep(5);
        // printf("Processo P7 finalizado com status: %d\n", z);
        close(pipe01[0]);
        close(pipe02[0]);
        for (int i = 0; i < 3; i++)
        {
            kill(produtores[i], 9);
            printf("Processo %d foi finalizado\n", produtores[i]);
        }
        kill(process4, 9);
        printf("Processo 4 foi finalizado\n");
        // kill(process5, 9);
        // printf("Processo 5 foi finalizado\n");
        // kill(process6, 9);
        // printf("Processo 6 foi finalizado\n");
        // kill(process7, 9); // se o wait funcionar retirar esse kill em p7
        // printf("Processo 7 foi finalizado\n");
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

                // write(pipe01[1], &res, sizeof(int));

                printf("Valor retirado da fila pela t1: %d\n", res);
                printf("Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
                if (shared_area_ptr_fifo1->queue_size == 0)
                {
                    ready_for_pickup = 0;
                    printf("PAUSOU T1, size = %d!!!\n", shared_area_ptr_fifo1->queue_size);
                    printf("Continuando processo: %d\n", shared_area_ptr_fifo1->paused_process);
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
        // sem_wait(&shared_area_ptr_fifo1->mutex1);
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

                // write(pipe02[1], &res, sizeof(int));

                printf("Valor retirado da fila pela t1: %d\n", res);
                printf("Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
                if (shared_area_ptr_fifo1->queue_size == 0)
                {
                    ready_for_pickup = 0;
                    printf("PAUSOU T2, size = %d!!!\n", shared_area_ptr_fifo1->queue_size);
                    printf("Continuando processo: %d\n", shared_area_ptr_fifo1->paused_process);
                    kill(shared_area_ptr_fifo1->paused_process, SIGUSR2);
                }
            }
        }
        sem_post(&shared_area_ptr_fifo1->mutex2);
    }
    printf("Finalizou T2\n");
}

// void *handleThreadsP7(void *ptr) {
//   struct shared_area_f2 *shared_area_ptr_fifo2;
//   shared_area_ptr_fifo2 = ((struct shared_area_f2*)ptr);

//   // sem_wait(&shared_area_ptr_fifo1->mutex2);
//   // printf("Entrou no handle T2\n");
//   // printf("T2: Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
//   // printf("T2: Turn: %d\n", shared_area_ptr_fifo1->turn);
//   // printf("T2: ready_for_pickup: %d\n", ready_for_pickup);
//   // sem_post(&shared_area_ptr_fifo1->mutex2);

//   for(;;) {
//     sem_wait(&shared_area_ptr_fifo1->mutex1);
//     sem_wait(&shared_area_ptr_fifo1->mutex2);

//     if (ready_for_pickup == 1 && shared_area_ptr_fifo1->turn == CONSUMER) {
//       if (shared_area_ptr_fifo1->queue_size > 0) {
//         int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

//         for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++) {
//           shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
//         } // reorganiza a fila após a exclusão
//         shared_area_ptr_fifo1->queue_size -= 1;

//         write(pipe02[1], &res, sizeof(int));

//         if (shared_area_ptr_fifo1->queue_size == 0) {
//           ready_for_pickup = 0;
//           shared_area_ptr_fifo1->turn = PRODUCER;
//         }
//         // printf("Valor retirado da fila pela t2: %d\n", res);
//         // printf("Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
//       }
//       sem_post(&shared_area_ptr_fifo1->mutex1);
//       sem_post(&shared_area_ptr_fifo1->mutex2);
//     } else {
//       // printf("PAUSOU T2, size = %d!!!\n\n\n", shared_area_ptr_fifo1->queue_size);
//       sem_post(&shared_area_ptr_fifo1->mutex1);
//       sem_post(&shared_area_ptr_fifo1->mutex2);
//       pause();
//     }
//   }
//   printf("Finalizou T2\n");
// }

void signalHandler(int p)
{
    printf("Threads consumindo!\n\n");
    ready_for_pickup = 1;
}

void signalHandler2(int p)
{
    printf("Produtores retornando..\n\n");
}

int rand_interval(int a, int b)
{
    return rand() % (b - a + 1) + a;
}
