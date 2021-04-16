#include <stdio.h>
#include <pthread.h>

#define NTHREADS 10

void *thread_function(void *);

int counter = 0;

int main() {
  pthread_t thread_id[NTHREADS];
  int i, j;

  for(i=0; i < NTHREADS; i++)
  {
    pthread_create( &thread_id[i], NULL, thread_function, NULL );
  }
  for(j=0; j < NTHREADS; j++)
  {
    pthread_join( thread_id[j], NULL);
  }
  printf("Valor final do contador: %d\n", counter);
}

void *thread_function(void *ptr) {
  printf("Thread nr. %ld\n", pthread_self());
  counter++;
  pthread_exit(NULL);
}