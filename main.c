#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <semaphore.h>

#define MINNUMBER 1
#define MAXNUMBER 1000
#define ITERACTIONS 10000
#define QUEUESIZE 10
#define MEM_SZ 4096
#define PRODUCER 1
#define CONSUMER 0

struct shared_area {
	sem_t mutex1; // Para os produtores escreverem
	sem_t mutex2; // Para regular retirada da fila entre t1 e t2 de P4
  int turn;
	int queue_size;
	int queue[QUEUESIZE];
};

void *handleT1(void *ptr);
void *handleT2(void *ptr);
int randInterval(int a, int b);
void signalHandler(int p);

int ready_for_pickup = 0;
int printedNumbers = 0;

int main() {
  pid_t produtores[3];
  pid_t process4;
  pid_t main_process = getpid();

  int pipe01[2];
  int pipe02[2];

  key_t fifo1 = 5678;
  struct shared_area *shared_area_ptr_fifo1;
  void *shared_memory_fifo1 = (void *)0;
  int shmid_fifo1;

  key_t fifo2 = 4538;
  struct shared_area *shared_area_ptr_fifo2;
  void *shared_memory_fifo2 = (void *)0;
  int shmid_fifo2;

  process4 = fork();

  if (process4 == -1) {
    printf("Erro ao criar o processo consumidor\n");
    exit(-1);
  } else if (process4 == 0) {
    signal(SIGUSR1, signalHandler);
    pthread_t threads[2];

    shmid_fifo1 = shmget(fifo1, MEM_SZ, 0666|IPC_CREAT);

    if (shmid_fifo1 == -1) {
      printf("shmget falhou ao criar fifo1 no processo 4\n");
      exit(-1);
    }

    shared_memory_fifo1 = shmat(shmid_fifo1,(void*)0,0);

    if (shared_memory_fifo1 == (void *) -1 ) {
      printf("shmat falhou nos produtores\n");
      exit(-1);
    }

    shared_area_ptr_fifo1 = (struct shared_area *) shared_memory_fifo1;

    shared_area_ptr_fifo1->turn = PRODUCER;
    shared_area_ptr_fifo1->queue_size = 0;

    if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex2, 0, 1) != 0 ) { // Cria um semáforo pra ser utilizado entre threads(t1,t2) (0)
      printf("sem_init falhou (FIFO 1, SEMAFORO 2)\n");
      exit(-1);
    }

    while(ready_for_pickup != 1);

    pthread_create(&threads[0], NULL, handleT1, (void *)shared_area_ptr_fifo1);
    pthread_create(&threads[1], NULL, handleT2, (void *)shared_area_ptr_fifo1);

    while (1);
  }

  for (int i = 0; i < 3; i++) {
    produtores[i] = fork();

    if (produtores[i] == -1) {
      printf("Erro ao criar os processos produtores\n");
      exit(-1);
    }
    if (produtores[i] == 0) {
      srand(time(NULL) + getpid());
      shmid_fifo1 = shmget(fifo1, MEM_SZ, 0666|IPC_CREAT);

      if (shmid_fifo1 == -1) {
        printf("shmget falhou\n");
        exit(-1);
      }

      shared_memory_fifo1 = shmat(shmid_fifo1,(void*)0,0);

      if (shared_memory_fifo1 == (void *) -1 ) {
        printf("shmat falhou nos produtores\n");
        exit(-1);
      }

      shared_area_ptr_fifo1 = (struct shared_area *) shared_memory_fifo1;

	    if (sem_init((sem_t *)&shared_area_ptr_fifo1->mutex1, 1, 1) != 0 ) { // Cria um semáforo pra ser utilizado entre processos (p1, p2, p3) (1)
        printf("sem_init falhou (FIFO 1, SEMAFORO 1)\n");
        exit(-1);
      }

      for(;;) {
        sem_wait((sem_t*)&shared_area_ptr_fifo1->mutex1);
        if (shared_area_ptr_fifo1->queue_size < 10 && shared_area_ptr_fifo1->turn == PRODUCER) {
          int x = randInterval(MINNUMBER, MAXNUMBER);
          shared_area_ptr_fifo1->queue[shared_area_ptr_fifo1->queue_size] = x; // Insere na última posição da fila
          shared_area_ptr_fifo1->queue_size += 1; // Soma um no tamanho atual

          printf("Número gerado: %d\n", x);
          printf("Tamanho da fila: %d\n", shared_area_ptr_fifo1->queue_size);
          printf("Turn: %d\n", shared_area_ptr_fifo1->turn);

          if (shared_area_ptr_fifo1->queue_size == 10) {
            printf("Fila encheu. %d\n", getpid());
            shared_area_ptr_fifo1->turn = CONSUMER;
            kill(process4, SIGUSR1);
          }
        }
        sem_post((sem_t*)&shared_area_ptr_fifo1->mutex1);
      }
    }
  }

  if(getpid() == main_process) {
    sleep(10);
    for (int i = 0; i < 3; i++) {
      kill(produtores[i], 9);
      printf("Processo %d foi finalizado\n", produtores[i]);
    }
    kill(process4, 9);
    printf("Processo 4 foi finalizado\n");
  }

  return 0;
}

// void *handleT1(void *ptr) {
//   struct shared_area *shared_area_ptr_fifo1;
//   shared_area_ptr_fifo1 = ((struct shared_area*)ptr);

//   printf("Entrou no handle T1\n");
//   printf("T1: Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
//   printf("T1: Turn: %d\n", shared_area_ptr_fifo1->turn);
//   printf("T1: ready_for_pickup: %d\n", ready_for_pickup);

//   while (ready_for_pickup == 1) {
//     sem_wait(&shared_area_ptr_fifo1->mutex2);

//     if (shared_area_ptr_fifo1->queue_size > 0) {
//       int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

//       for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++) {
//         shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
//       } // reorganiza a fila após a exclusão
//       shared_area_ptr_fifo1->queue_size--;
//       // TODO Mandar para o pipe01;
//       if (shared_area_ptr_fifo1->queue_size == 0) {
//         ready_for_pickup = 0;
//         shared_area_ptr_fifo1->turn = PRODUCER;
//       }
//       printf("Valor retirado da fila pela t1: %d\n", res);
//       printedNumbers++;
//       printf("Printed numbers: %d\n", printedNumbers);
//     }
//     sem_post(&shared_area_ptr_fifo1->mutex2);

//     if (ready_for_pickup == 0) {
//       printf("PAUSOU T1!!!\n\n\n");
//       pause();
//     }
//   }
//   printf("Finalizou T1\n");
// }
void *handleT1(void *ptr) {
  struct shared_area *shared_area_ptr_fifo1;
  shared_area_ptr_fifo1 = ((struct shared_area*)ptr);

  sem_wait(&shared_area_ptr_fifo1->mutex2);
  printf("Entrou no handle T1\n");
  printf("T1: Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
  printf("T1: Turn: %d\n", shared_area_ptr_fifo1->turn);
  printf("T1: ready_for_pickup: %d\n", ready_for_pickup);
  sem_post(&shared_area_ptr_fifo1->mutex2);

  for(;;) {
    sem_wait(&shared_area_ptr_fifo1->mutex1);
    sem_wait(&shared_area_ptr_fifo1->mutex2);

    if (ready_for_pickup == 1 && shared_area_ptr_fifo1->turn == CONSUMER) {
      if (shared_area_ptr_fifo1->queue_size > 0) {
        int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

        for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++) {
          shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
        } // reorganiza a fila após a exclusão
        shared_area_ptr_fifo1->queue_size -= 1;
        // TODO Mandar para o pipe02;
        if (shared_area_ptr_fifo1->queue_size == 0) {
          ready_for_pickup = 0;
          shared_area_ptr_fifo1->turn = PRODUCER;
        }
        printedNumbers++;
        printf("Valor retirado da fila pela t1: %d\n", res);
        printf("Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
      }
      sem_post(&shared_area_ptr_fifo1->mutex1);
      sem_post(&shared_area_ptr_fifo1->mutex2);
    } else {
      printf("PAUSOU T1, size = %d!!!\n\n\n", shared_area_ptr_fifo1->queue_size);
      sem_post(&shared_area_ptr_fifo1->mutex1);
      sem_post(&shared_area_ptr_fifo1->mutex2);
      pause();
    }
  }
  printf("Finalizou T1\n");
}

void *handleT2(void *ptr) {
  struct shared_area *shared_area_ptr_fifo1;
  shared_area_ptr_fifo1 = ((struct shared_area*)ptr);

  sem_wait(&shared_area_ptr_fifo1->mutex2);
  printf("Entrou no handle T2\n");
  printf("T2: Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
  printf("T2: Turn: %d\n", shared_area_ptr_fifo1->turn);
  printf("T2: ready_for_pickup: %d\n", ready_for_pickup);
  sem_post(&shared_area_ptr_fifo1->mutex2);

  for(;;) {
    sem_wait(&shared_area_ptr_fifo1->mutex1);
    sem_wait(&shared_area_ptr_fifo1->mutex2);

    if (ready_for_pickup == 1 && shared_area_ptr_fifo1->turn == CONSUMER) {
      if (shared_area_ptr_fifo1->queue_size > 0) {
        int res = shared_area_ptr_fifo1->queue[0]; // Pega o primeiro da fila

        for (int i = 0; i < shared_area_ptr_fifo1->queue_size - 1; i++) {
          shared_area_ptr_fifo1->queue[i] = shared_area_ptr_fifo1->queue[i + 1];
        } // reorganiza a fila após a exclusão
        shared_area_ptr_fifo1->queue_size -= 1;
        // TODO Mandar para o pipe02;
        if (shared_area_ptr_fifo1->queue_size == 0) {
          ready_for_pickup = 0;
          shared_area_ptr_fifo1->turn = PRODUCER;
        }
        printedNumbers++;
        printf("Valor retirado da fila pela t2: %d\n", res);
        printf("Queue size: %d\n", shared_area_ptr_fifo1->queue_size);
      }
      sem_post(&shared_area_ptr_fifo1->mutex1);
      sem_post(&shared_area_ptr_fifo1->mutex2);
    } else {
      printf("PAUSOU T2, size = %d!!!\n\n\n", shared_area_ptr_fifo1->queue_size);
      sem_post(&shared_area_ptr_fifo1->mutex1);
      sem_post(&shared_area_ptr_fifo1->mutex2);
      pause();
    }
  }
  printf("Finalizou T2\n");
}

void signalHandler(int p) {
	ready_for_pickup = 1;
  printf("Sinal enviado\n");
}

int randInterval(int a, int b) {
  return rand() % (b - a + 1) + a;
}
