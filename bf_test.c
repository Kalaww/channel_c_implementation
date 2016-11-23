#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "brute_force.h"
#include "channel.h"

struct brute_force *bf;
struct channel *chan;
int done;

#define MASTERS 1
#define SLAVES 1
#define CHANNEL_SIZE 100
#define CHANNEL_FLAGS 0

void *fcn_master() {
  char* current;
  char* next;

  current = combination_create(bf);

  do {
    if(channel_send(chan, (void *)current) == -1) {
      perror("channel_send");
      exit(1);
    }

    next = (char *)malloc(sizeof(char)*strlen(current));
    combination_next(bf, current, next);
    free(current);

    current = next;
  }
  while(strcmp(current, combination_create(bf)) && !done);

  channel_close(chan);
  return NULL;
}

void *fcn_slave(void *size) {
  char* comb;

  while(1) {
    comb = (char *)malloc(sizeof(char)*get_size(bf));

    if(channel_recv(chan, (void *)comb) <= 0) {
      break;
    }
    if(!combination_equals(bf, comb)) {
      done = 1;
    }

    free(comb);
  }

  return NULL;
}

int bf_test(int size, char type, int flags) {
  bf = password_create(size, type, flags);
  chan = channel_create(sizeof(char)*get_size(bf), CHANNEL_SIZE, CHANNEL_FLAGS);
  if(chan == NULL){
    perror("channel_create");
    exit(1);
  }
  pthread_t master[MASTERS], slave[SLAVES];
  int i;

  done = 0;

  for(i = 0; i < MASTERS; i++) {
    pthread_create(&master[i], NULL, fcn_master, NULL);
  }

  for(i = 0; i < SLAVES; i++) {
    pthread_create(&slave[i], NULL, fcn_slave, NULL);
  }

  for(i = 0; i < MASTERS; i++) {
    pthread_join(master[i], NULL);
  }

  for(i = 0; i < SLAVES; i++) {
    pthread_join(slave[i], NULL);
  }
  
  channel_destroy(chan);

  return 0;
}

int main(int argc, char *argv[]) {
  int size = 0, i, flags = 0;
  char type = 'f';
  struct timespec t0, t1;

  for(i = 1; i < argc; i++) {
    if((!strcmp(argv[i], "--size") || !strcmp(argv[i], "-s")) && i < argc-1) {
      size = atoi(argv[i+1]);
    }
    else if((!strcmp(argv[i], "--type") || !strcmp(argv[i], "-t")) && i < argc-1) {
      type = *argv[i+1];
    }
    else if(!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d")) {
      flags++;
    }
  }


  clock_gettime(CLOCK_MONOTONIC, &t0);
  bf_test(size, type, flags);
  clock_gettime(CLOCK_MONOTONIC, &t1);

  printf("done: %.5fs\n",
    ((double) t1.tv_sec + 1.0e-9*t1.tv_nsec) -
    ((double) t0.tv_sec + 1.0e-9*t0.tv_nsec));

  return 0;
}