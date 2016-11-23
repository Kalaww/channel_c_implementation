#ifndef BRUTE_FORCE_H
#define BRUTE_FORCE_H

struct brute_force;

#define DEFAULT_SIZE 6
#define DEFAULT_TYPE 'n'

struct brute_force *password_create(int size, char type, int flags);
char *combination_create(struct brute_force *bf);
int combination_equals(struct brute_force *bf, char *comb);
char *combination_next(struct brute_force *bf, char *src, char *dest);
int get_size(struct brute_force *bf);
char get_type(struct brute_force *bf);

#endif