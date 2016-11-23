#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "channel.h"

typedef struct args{
    int nb_producteur;
    int nb_consommateur;
    int element_size;
    int channel_size;
    int channel_flags;
    int nb_send_total;
    int est_par_lot_producteur;
    int est_par_lot_consommateur;
    int taille_lot_producteur;
    int taille_lot_consommateur;
} _args;

typedef struct args_producteur{
    struct channel* chan;
    int element_size;
    int* nb_send;
    int est_par_lot;
    int taille_lot;
} _args_producteur;

typedef struct args_consommateur{
    struct channel* chan;
    int element_size;
    int est_par_lot;
    int taille_lot;
} _args_consommateur;

typedef struct args_prod_tube{
    int* nb_send;
    int* tube;
    int element_size;
} _args_prod_tube;

typedef struct args_conso_tube{
    int* tube;
    int element_size;
} _args_conso_tube;


double time_diff(struct timespec tstart, struct timespec tend){
    return ((double) tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double) tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
}

struct args *args_init(){
    struct args *a;

    a = (struct args *) malloc(sizeof(struct args));

    a->nb_producteur = 0;
    a->nb_consommateur = 0;
    a->element_size = 1;
    a->channel_size = 1;
    a->channel_flags = 0;
    a->nb_send_total = 1;
    a->est_par_lot_producteur = 0;
    a->est_par_lot_consommateur = 0;
    a->taille_lot_producteur = 0;
    a->taille_lot_consommateur = 0;

    return a;
}

void* fct_producteur(void* p){
    struct args_producteur* args = (struct args_producteur *) p;

    void* data;

    if(args->est_par_lot) {
        data = malloc(args->element_size * args->taille_lot);
        memset(data, 0, args->element_size * args->taille_lot);
    }else {
        data = malloc(args->element_size);
        memset(data, 0, args->element_size);
    }

    while( (*(args->nb_send)) > 0){
        if(args->est_par_lot){
            int nb_send = 0;
            nb_send = channel_send_n(args->chan, &data, args->taille_lot);

            if(nb_send < 0){
                perror("produteur send n channel");
                return NULL;
            }
            if(nb_send == 0)
                return NULL;
        }else{
            if(channel_send(args->chan, (void*)data) == -1){
                perror("producteur send channel");
                return NULL;
            }
        }
        (*(args->nb_send))--;
    }

    free(data);

    return NULL;
}

void* fct_consommateur(void* p){
    struct args_consommateur* args = (struct args_consommateur *) p;

    void* data;

    if(args->est_par_lot) {
        data = malloc(args->element_size * args->taille_lot);
        memset(data, 0, args->element_size * args->taille_lot);
    }else {
        data = malloc(args->element_size);
        memset(data, 0, args->element_size);
    }

    while(1){
        if(args->est_par_lot){
            int nb_recv;
            nb_recv = channel_recv_n(args->chan, &data, args->taille_lot);

            if(nb_recv < 0){
                perror("consommateur send n channel");
                return NULL;
            }
            if(nb_recv == 0)
                return NULL;
        }else{
            if(channel_recv(args->chan, (void*)data) <= 0)
                break;
        }
    }

    free(data);

    return NULL;
}

int test_producteur_consommateur(struct args *args){
    int i;
    int* nb_send;
    pthread_t producteurs[args->nb_producteur], consommateurs[args->nb_consommateur];
    struct channel* chan;
    struct args_producteur* args_producteur[args->nb_producteur];
    struct args_consommateur* args_consommateur[args->nb_consommateur];

    chan = channel_create(args->element_size, args->channel_size, args->channel_flags);
    if(chan == NULL){
        perror("channel create");
        return 1;
    }

    nb_send = (int*) malloc(sizeof(int));
    *nb_send = args->nb_send_total;

    for(i = 0; i < args->nb_producteur; i++){
        args_producteur[i] = (struct args_producteur *) malloc(sizeof(struct args_producteur));
        args_producteur[i]->chan = chan;
        args_producteur[i]->element_size = args->element_size;
        args_producteur[i]->nb_send = nb_send;
        args_producteur[i]->est_par_lot = args->est_par_lot_producteur;
        args_producteur[i]->taille_lot = args->taille_lot_producteur;
        pthread_create(&producteurs[i], NULL, fct_producteur, args_producteur[i]);
    }

    for(i = 0; i < args->nb_consommateur; i++){
        args_consommateur[i] = (struct args_consommateur *) malloc(sizeof(struct args_consommateur));
        args_consommateur[i]->chan = chan;
        args_consommateur[i]->element_size = args->element_size;
        args_consommateur[i]->est_par_lot = args->est_par_lot_consommateur;
        args_consommateur[i]->taille_lot = args->taille_lot_consommateur;
        pthread_create(&consommateurs[i], NULL, fct_consommateur, args_consommateur[i]);
    }

    for(i = 0; i < args->nb_producteur; i++){
        pthread_join(producteurs[i], NULL);
        free(args_producteur[i]);
    }

    channel_close(chan);

    for(i = 0; i < args->nb_consommateur; i++){
        pthread_join(consommateurs[i], NULL);
        free(args_consommateur[i]);
    }

    channel_destroy(chan);
    free(nb_send);

    return 0;
}

void* fct_prod_tube(void* p){
    struct args_prod_tube *args = (struct args_prod_tube*) p;
    void* data;
    int rd;

    data = malloc(args->element_size);
    memset(data, 0, args->element_size);

    while((*(args->nb_send)) > 0){
        rd = write(args->tube[1], data, args->element_size);

        if(rd <= 0)
            break;

        (*(args->nb_send))--;
    }

    free(data);

    return NULL;
}

void* fct_conso_tube(void* p){
    struct args_conso_tube *args = (struct args_conso_tube*) p;
    void* data;
    int rd;

    data = malloc(args->element_size);
    memset(data, 0, args->element_size);

    while(1){
        rd = read(args->tube[0], data, args->element_size);

        if(rd <= 0)
            break;

    }

    free(data);

    return NULL;
}

int test_prod_conso_tube(struct args *args){
    int i;
    int* nb_send;
    pthread_t producteurs[args->nb_producteur], consommateurs[args->nb_consommateur];
    struct args_prod_tube* args_prod[args->nb_producteur];
    struct args_conso_tube* args_conso[args->nb_consommateur];

    int tube[2];

    pipe(tube);

    nb_send = (int*) malloc(sizeof(int));
    *nb_send = args->nb_send_total;

    for(i = 0; i < args->nb_producteur; i++){
        args_prod[i] = (struct args_prod_tube *) malloc(sizeof(struct args_prod_tube));
        args_prod[i]->nb_send = nb_send;
        args_prod[i]->tube = tube;
        args_prod[i]->element_size = args->element_size;
        pthread_create(&producteurs[i], NULL, fct_prod_tube, args_prod[i]);
    }

    for(i = 0; i < args->nb_consommateur; i++){
        args_conso[i] = (struct args_conso_tube *) malloc(sizeof(struct args_conso_tube));
        args_conso[i]->tube = tube;
        args_conso[i]->element_size = args->element_size;
        pthread_create(&consommateurs[i], NULL, fct_conso_tube, args_conso[i]);
    }

    for(i = 0; i < args->nb_producteur; i++){
        pthread_join(producteurs[i], NULL);
        free(args_prod[i]);
    }

    close(tube[1]);

    for(i = 0; i < args->nb_consommateur; i++){
        pthread_join(consommateurs[i], NULL);
        free(args_conso[i]);
    }

    close(tube[0]);

    return 0;
}

void test_tube(struct args* args, int nb_test){
    struct timespec tstart = {0,0}, tend = {0,0};
    double somme;
    int i;

    i = 0;
    somme = 0.0;

    while(i < nb_test){
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        test_prod_conso_tube(args);
        clock_gettime(CLOCK_MONOTONIC, &tend);

        somme += time_diff(tstart, tend);
        i++;
    }
    printf("%.5f\n",(somme / (double)nb_test));

    return;
}

void print_args(struct args *args){
    printf("producteurs: %d | consommateurs: %d\n", args->nb_producteur, args->nb_consommateur);
    printf("nb send par producteur: %d\n", args->nb_send_total);
    printf("element size: %d\n", args->element_size);
    printf("channel size: %d\n", args->channel_size);

    printf("channel flags: ");
    if((args->channel_flags & CHANNEL_PROCESS_SHARED) == 1)
        printf("canal global");
    else
        printf("none");
    printf("\n");

    if(args->est_par_lot_producteur){
        printf("est par lot producteur: TRUE\n\ttaille lot: %d\n",
            args->taille_lot_producteur);
    }else
        printf("est par lot producteur: FALSE\n");

    if(args->est_par_lot_consommateur){
        printf("est par lot consommateur: TRUE\n\ttaille lot: %d\n",
               args->taille_lot_consommateur);
    }else
        printf("est par lot consommateur: FALSE\n");
}


double run_test(struct args *args){
    struct timespec tstart = {0,0}, tend = {0,0};

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    test_producteur_consommateur(args);
    clock_gettime(CLOCK_MONOTONIC, &tend);

    return time_diff(tstart, tend);
}

void test_asynchrone(struct args* args, int nb_test){
    double somme;
    int i;

    i = 0;
    somme = 0.0;

    if(args->nb_consommateur == 6)
        args->nb_consommateur = 5;

    while(i < nb_test){
        somme += run_test(args);
        i++;
    }
    printf("%.5f\n",(somme / (double) nb_test));

    return;
}

void test_synchrone(struct args* args, int nb_test){
    double somme;
    int i;

    i = 0;
    somme = 0.0;

    while(i < nb_test){
        somme += run_test(args);
        i++;
    }
    printf("%.5f\n",(somme / (double) nb_test));

    return;
}

void test_communication_par_lot(struct args* args, int nb_test){
    double somme;
    int i;

    i = 0;
    somme = 0.0;

    while(i < nb_test){
        somme += run_test(args);
        i++;
    }
    printf("%.5f\n",(somme / (double) nb_test));

    return;
}

int main(int argc, char** argv){
    int i, nb_test;
    struct args* args;

    args = args_init();

    i = 1;
    nb_test = 1;

    while(i < argc){
        if(strcmp(argv[i], "-t") == 0){
            i++;
            nb_test = atoi(argv[i]);
        }else if(strcmp(argv[i], "-async") == 0){
            i++;
            args->nb_producteur = atoi(argv[i]);
            i++;
            args->nb_consommateur = atoi(argv[i]);
            i++;
            args->channel_size = atoi(argv[i]);
            i++;
            args->element_size = atoi(argv[i]);
            i++;
            args->nb_send_total = atoi(argv[i]);

            test_asynchrone(args, nb_test);
        }else if(strcmp(argv[i], "-sync") == 0) {
            i++;
            args->nb_producteur = atoi(argv[i]);
            i++;
            args->nb_consommateur = atoi(argv[i]);
            i++;
            args->element_size = atoi(argv[i]);
            i++;
            args->nb_send_total = atoi(argv[i]);

            test_synchrone(args, nb_test);
        }else if(strcmp(argv[i], "-lots") == 0) {
            i++;
            args->nb_producteur = atoi(argv[i]);
            i++;
            args->nb_consommateur = atoi(argv[i]);
            i++;
            args->channel_size = atoi(argv[i]);
            i++;
            args->element_size = atoi(argv[i]);
            i++;
            args->nb_send_total = atoi(argv[i]);
            i++;
            args->est_par_lot_producteur = atoi(argv[i]);
            i++;
            args->taille_lot_producteur = atoi(argv[i]);
            i++;
            args->est_par_lot_consommateur = atoi(argv[i]);
            i++;
            args->taille_lot_consommateur = atoi(argv[i]);

            test_communication_par_lot(args, nb_test);
        }else if(strcmp(argv[i], "-tube") == 0){
            i++;
            args->nb_producteur = atoi(argv[i]);
            i++;
            args->nb_consommateur = atoi(argv[i]);
            i++;
            args->element_size = atoi(argv[i]);
            i++;
            args->nb_send_total = atoi(argv[i]);

            test_tube(args, nb_test);
        }
        i++;
    }

    free(args);

    return 0;
}