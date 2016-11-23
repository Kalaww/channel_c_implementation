#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "brute_force.h"

struct brute_force {
  int size;
  char type;
  char *pswd;
};

void debug(struct brute_force *bf) {
  printf("size: %d\ntype: %c\npswd: %s\n", bf->size, bf->type, bf->pswd);
}

struct brute_force *password_create(int size, char type, int flags) {
  struct brute_force* bf = (struct brute_force *)malloc(sizeof(struct brute_force));
  int i;

  srand(time(NULL));

  if (size <= 0) {
    bf->size = DEFAULT_SIZE;
  }
  else {
    bf->size = size;
  }

  if (type != 'n' && type != 'l' && type != 'u' && type != 'a') {
    bf->type = DEFAULT_TYPE;
  }
  else {
    bf->type = type;
  }

  bf->pswd = malloc(sizeof(char)*bf->size);
  switch(bf->type) {
    case 'n':
      for(i = 0; i < bf->size; i++) {
        bf->pswd[i] = rand()%9+48;
      }
      break;
    case 'l':
      for(i = 0; i < bf->size; i++) {
        bf->pswd[i] = rand()%26+97;
      }
      break;
    case 'u':
      for(i = 0; i < bf->size; i++) {
        bf->pswd[i] = rand()%26+65;
      }
      break;
    case 'a':
      for(i = 0; i < bf->size; i++) {
        switch(rand() % 3) {
          case 0:
            bf->pswd[i] = rand()%9+48;
            break;
          case 1:
            bf->pswd[i] = rand()%26+97;
            break;
          case 2:
            bf->pswd[i] = rand()%26+65;
            break;
        }
      }
      break;
  }

  if(flags) {
    debug(bf);
  }

  return bf;
}

char *combination_create(struct brute_force *bf) {
  char *comb = malloc(sizeof(char)*bf->size);
  int i;

  switch(bf->type) {
    case 'l':
      for(i = 0; i < bf->size; i++) {
        comb[i] = 'a';
      }
      break;
    case 'u':
      for(i = 0; i < bf->size; i++) {
        comb[i] = 'A';
      }
      break;
    default:
      for(i = 0; i < bf->size; i++) {
        comb[i] = '0';
      }
      break;
  }

  return comb;
}

int combination_equals(struct brute_force *bf, char *comb) {
  int i;

  for(i = 0; i < bf->size; i++) {
    if(comb[i] != bf->pswd[i]) {
      return 1;
    }
  }
  return 0;
}

char *combination_next(struct brute_force *bf, char *src, char* dest) {
  int i;

  memcpy(dest, src, bf->size);

  for(i = bf->size-1; i >= 0; i--) {
    if(dest[i] == '9') {
      if(bf->type == 'n') {
        dest[i] = '0';
      }
      else {
        dest[i] = 'a';
        break;
      }
    }
    else if(dest[i] == 'z') {
      if(bf->type == 'l') {
        dest[i] = 'a';
      }
      else {
        dest[i] = 'A';
        break;
      }
    }
    else if(dest[i] == 'Z') {
      if(bf->type == 'u') {
        dest[i] = 'A';
      }
      else {
        dest[i] = '0';
      }
    }
    else {
      dest[i]++;
      break;
    }
  }
  return dest;
}

int get_size(struct brute_force *bf) {
  return bf->size;
}

char get_type(struct brute_force *bf) {
  return bf->type;
}