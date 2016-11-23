#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

#include "channel.h"

#ifdef DEBUG
#define DEBUG_V 1
#else
#define DEBUG_V 0
#endif

#define debug_print(fmt, ...) \
    do{ if(DEBUG_V) fprintf(stderr, fmt, __VA_ARGS__); }while (0)


typedef struct channel{
    void* memory;               // pointeur du début de l'espace mémoire du channel
    int head_send;              // position courante pour send
    int head_recv;              // position courante pour recv
    int nb_elt;                 // nombre d'éléments courament dans le channel
    int eltsize;                // taille d'un élément
    int size;                   // nombre d'élément que peut contenir le channel au maximum
    int is_open;                // boolean pour savoir si le channel est ouvert ou fermé
    pthread_mutex_t mutex;      // mutex du channel
    pthread_cond_t cond_mem_empty;          // variable condition mémoire pleine
    pthread_cond_t cond_mem_full;           // variable condition mémoire vide
    pthread_cond_t cond_sync_wait_recver;   // variable condition canaux synchrone en attente d'un receiver
} _channel;


#define CHANNEL_OPENED 1        // le channel est ouvert
#define CHANNEL_CLOSED 2        // le channel est fermé


int channel_lock(struct channel* channel);
int channel_unlock(struct channel* channel);

int channel_full_check(struct channel* channel, int taille_lot);
int channel_empty_check(struct channel* channel, int taille_lot);

int channel_signal_others(struct channel* channel);

struct channel *channel_create_async(int eltsize, int size, int flags);
struct channel *channel_create_sync(int eltsize, int flags);
int channel_send_async(struct channel *channel, const void *data);
int channel_send_sync(struct channel *channel, void *data);
int channel_recv_async(struct channel *channel, void *data);
int channel_recv_sync(struct channel *channel, void *data);

int channel_lock(struct channel* channel){
    debug_print("%lu: ask lock\n", pthread_self());
    if(pthread_mutex_lock(&channel->mutex) != 0){
        perror("channel lock");
        return -1;
    }
    return 1;
}

int channel_unlock(struct channel* channel){
    debug_print("%lu: unlock\n", pthread_self());
    if(pthread_mutex_unlock(&channel->mutex) != 0){
        perror("channel unlock");
        return -1;
    }
    return 1;
}

int channel_signal_others(struct channel* channel){
    int size;

    if(channel->size <= 0)
        size = 1;
    else
        size = channel->size;

    if(channel->nb_elt > 0 || (channel->is_open == CHANNEL_CLOSED && channel->nb_elt == 0)){
        debug_print("%lu: signal on recv\n", pthread_self());
        pthread_cond_signal(&channel->cond_mem_empty);
    }

    if(channel->nb_elt < size){
        debug_print("%lu: signal on send\n", pthread_self());
        pthread_cond_signal(&channel->cond_mem_full);
    }

    return 1;
}

int channel_full_check(struct channel* channel, int taille_lot){
    int size;

    if(taille_lot < 0)
        taille_lot = 0;

    if(channel->size <= 0)
        size = 1;
    else
        size = channel->size - taille_lot;

    while(channel->nb_elt == size){
        // si channel fermé, on stop
        if(channel->is_open == CHANNEL_CLOSED){
            channel_signal_others(channel);
            channel_unlock(channel);
            errno = EPIPE;
            return -1;
        }

        debug_print("%lu: wait lock\n", pthread_self());
        if(pthread_cond_wait(&channel->cond_mem_full, &channel->mutex) != 0){
            perror("waitting on cond_mem_full");
            channel_unlock(channel);
            return -1;
        }

        // si channel fermé, on stop
        if(channel->is_open == CHANNEL_CLOSED){
            channel_signal_others(channel);
            channel_unlock(channel);
            errno = EPIPE;
            return -1;
        }
    }

    debug_print("%lu: have lock\n", pthread_self());
    return 1;
}

int channel_empty_check(struct channel* channel, int taille_lot){
    if(taille_lot < 0)
        taille_lot = 1;

    //debug_print("%lu: while\n", pthread_self());
    while((taille_lot > 1 && channel->is_open == CHANNEL_OPENED && channel->nb_elt < taille_lot) ||
          (taille_lot == 1 && channel->nb_elt < 1)){
        // si channel fermé, on stop
        if(channel->is_open == CHANNEL_CLOSED && channel->nb_elt <= 0){
            debug_print("channel closed a t:%lu\n", pthread_self());
            channel_signal_others(channel);
            channel_unlock(channel);
            return 0;
        }

        debug_print("%lu: wait lock c:%d nb:%d\n", pthread_self(), channel->is_open, channel->nb_elt);
        if(pthread_cond_wait(&channel->cond_mem_empty, &channel->mutex) != 0){
            perror("waitting on cond_mem_empty");
            channel_unlock(channel);
            return -1;
        }

        // si channel fermé, on stop
        if(channel->is_open == CHANNEL_CLOSED && channel->nb_elt <= 0){
            debug_print("channel closed b t:%lu\n", pthread_self());
            channel_signal_others(channel);
            channel_unlock(channel);
            return 0;
        }
    }

    debug_print("%lu: have lock\n", pthread_self());
    return 1;
}


struct channel *channel_create(int eltsize, int size, int flags){
    if(size <= 0){
        return channel_create_sync(eltsize, flags);
    }else{
        return channel_create_async(eltsize, size, flags);
    }
}


struct channel *channel_create_async(int eltsize, int size, int flags){
    int mmap_flags;

    struct channel* chan = (struct channel *) malloc(sizeof(struct channel));
    if(chan == NULL){
        perror("malloc");
        return NULL;
    }

    chan->nb_elt = 0;
    chan->eltsize = eltsize;
    chan->size = size;
    chan->is_open = CHANNEL_OPENED;
    chan->head_send = 0;
    chan->head_recv = 0;

    pthread_mutex_init(&chan->mutex, NULL);
    pthread_cond_init(&chan->cond_mem_full, NULL);
    pthread_cond_init(&chan->cond_mem_empty, NULL);

    mmap_flags = MAP_ANONYMOUS;
    // Canaux globaux
    if((flags & CHANNEL_PROCESS_SHARED) == 1){
        mmap_flags = mmap_flags | MAP_SHARED;
    }else{ // Canaux non globaux
        mmap_flags = mmap_flags | MAP_PRIVATE;
    }

    chan->memory = mmap(NULL,
                        eltsize * size,
                        PROT_READ | PROT_WRITE,
                        mmap_flags,
                        -1, 0);

    if(chan->memory == MAP_FAILED){
        perror("mmap");
        pthread_mutex_destroy(&chan->mutex);
        free(chan);
        return NULL;
    }

    return chan;
}


struct channel *channel_create_sync(int eltsize, int flags) {
    struct channel *chan = (struct channel *) malloc(sizeof(struct channel));
    if (chan == NULL) {
        perror("malloc");
        return NULL;
    }

    chan->nb_elt = 0;
    chan->eltsize = eltsize;
    chan->size = 0;
    chan->is_open = CHANNEL_OPENED;
    chan->head_send = 0;
    chan->head_recv = 0;

    pthread_mutex_init(&chan->mutex, NULL);
    pthread_cond_init(&chan->cond_mem_full, NULL);
    pthread_cond_init(&chan->cond_mem_empty, NULL);
    pthread_cond_init(&chan->cond_sync_wait_recver, NULL);

    chan->memory = NULL;

    return chan;
}


void channel_destroy(struct channel *channel){
    if(channel == NULL)
        return;

    // canaux asynchrone
    if(channel->size > 0){
        if(munmap(channel->memory, channel->eltsize * channel->size) == -1){
            perror("munmap");
        }
    }

    pthread_mutex_destroy(&channel->mutex);

    free(channel);
}


int channel_close(struct channel *channel){
    // si channel est null
    if(channel == NULL){
        errno = EINVAL;
        return -1;
    }

    channel_lock(channel);

    // si déjà fermé
    if(channel->is_open == CHANNEL_CLOSED){
        return 0;
    }

    // ferme le channel
    channel->is_open = CHANNEL_CLOSED;

    debug_print("%lu: channel closed\n", pthread_self());
    channel_signal_others(channel);

    channel_unlock(channel);

    return 1;
}


int channel_send(struct channel *channel, void *data){
    int return_value;

    // channel est null
    if(channel == NULL){
        errno = EINVAL;
        return -1;
    }

    // le channel est fermé
    if(channel->is_open == CHANNEL_CLOSED){
        errno = EPIPE;
        return -1;
    }

    return_value = channel_lock(channel);
    if(return_value != 1)
        return return_value;

    return_value = channel_full_check(channel, -1);
    if(return_value != 1)
        return return_value;

    // synchrone
    if(channel->size <= 0){
        return_value =  channel_send_sync(channel, data);
    }else{ // asynchrone
        return_value = channel_send_async(channel, data);
    }

    if(return_value != 1)
        return return_value;

    channel_signal_others(channel);

    return channel_unlock(channel);
}


int channel_send_async(struct channel *channel, const void *data){
    // pointeur vers le prochain emplacement mémoire libre pour data
    void* ptr = channel->memory + (channel->eltsize * channel->head_send);

    // copie de data dans le channel
    memcpy(ptr, data, channel->eltsize);

    // inscremente la position head_send
    channel->head_send = (channel->head_send + 1) % channel->size;

    // incremente le compteur nb_elt
    channel->nb_elt++;

    debug_print("%lu: send_async: p:%p v:%d\n", pthread_self(), ptr, (*(int*) ptr));

    return 1;
}


int channel_send_sync(struct channel *channel, void *data){
    // memory pointe vers l'espace mémoire de la valeur à envoyer
    channel->memory = data;

    // inscremente le compteur nb_elt
    channel->nb_elt = 1;

    channel_signal_others(channel);

    debug_print("%lu: send_sync: p:%p v:%d\n", pthread_self(), channel->memory, (*(int*) channel->memory));

    // le sender se met en attente
    if(pthread_cond_wait(&channel->cond_sync_wait_recver, &channel->mutex) != 0){
        perror("waitting on cond_sync_wait_recver");
        channel_unlock(channel);
        return -1;
    }
    debug_print("%lu: send_sync: wake up\n", pthread_self());

    return 1;
}


int channel_recv(struct channel *channel, void *data){
    int return_value;

    if (channel == NULL) {
        errno = EINVAL;
        return -1;
    }

    debug_print("%lu: recv start nb_elt:%d close:%d\n", pthread_self(), channel->nb_elt, channel->is_open);
    // si le channel est fermé et qu'il n'y a plus rien de stocké dedans
    if (channel->is_open == CHANNEL_CLOSED && channel->nb_elt <= 0) {
        return 0;
    }

    return_value = channel_lock(channel);
    if(return_value != 1)
        return return_value;

    return_value = channel_empty_check(channel, -1);
    if(return_value != 1)
        return return_value;

    // synchrone
    if(channel->size <= 0){
        return_value = channel_recv_sync(channel, data);
    }else { // asynchrone
        return_value = channel_recv_async(channel, data);
    }

    if(return_value != 1)
        return return_value;

    channel_signal_others(channel);

    return channel_unlock(channel);
}


int channel_recv_async(struct channel *channel, void *data){
    // pointeur vers le prochain emplacement mémoire à lire
    void *ptr = channel->memory + (channel->eltsize * channel->head_recv);

    // copie de ptr dans data
    memcpy(data, ptr, channel->eltsize);

    // incremente la position head_recv
    channel->head_recv = (channel->head_recv + 1) % channel->size;

    // décremente le compteur nb_elt
    channel->nb_elt--;

    debug_print("%lu:\trecv_async: p:%p v:%d\n", pthread_self(), data, (*(int*) ptr));

    return 1;
}


int channel_recv_sync(struct channel *channel, void *data){
    // copie de memory dans data
    memcpy(data, channel->memory, channel->eltsize);

    // décremente le compteur nb_elt
    channel->nb_elt = 0;

    debug_print("%lu:\trecv_sync: p:%p v:%d\n", pthread_self(), channel->memory, (*(int*) channel->memory));

    channel->memory = NULL;

    pthread_cond_signal(&channel->cond_sync_wait_recver);

    return 1;
}


int channel_send_n(struct channel *channel, void **datas, int nb){
    int i, return_value, ret_total;

    if(nb <= 0){
        errno = EINVAL;
        return -1;
    }

    if(channel->size <= 0){
        errno = EINVAL;
        perror("channel_send_n non compatible avec un canal synchrone");
        return -1;
    }

    // le channel est fermé
    if(channel->is_open == CHANNEL_CLOSED){
        errno = EPIPE;
        return -1;
    }

    return_value = channel_lock(channel);
    if(return_value != 1)
        return return_value;

    return_value = channel_full_check(channel, nb);
    if(return_value != 1)
        return return_value;

    ret_total = 0;
    for(i = 0; i < nb; i++){
        if(channel->nb_elt >= channel->size)
            break;

        return_value = channel_send_async(channel, (*datas) + channel->eltsize * i);

        if(return_value != 1)
            return return_value;

        ret_total++;
    }

    channel_signal_others(channel);

    return_value = channel_unlock(channel);
    if(return_value != 1)
        return  return_value;

    return ret_total;
}


int channel_recv_n(struct channel *channel, void **datas, int nb){
    int i, return_value, ret_total;

    if (channel == NULL) {
        errno = EINVAL;
        return -1;
    }

    if(nb <= 0){
        errno = EINVAL;
        return -1;
    }

    if(channel->size <= 0){
        errno = EINVAL;
        perror("channel_recv_n non compatible avec un canal synchrone");
        return -1;
    }

    // si le channel est fermé et qu'il n'y a plus rien de stocké dedans
    if (channel->is_open == CHANNEL_CLOSED && channel->nb_elt == 0) {
        return 0;
    }

    return_value = channel_lock(channel);
    if(return_value != 1)
        return return_value;

    return_value = channel_empty_check(channel, nb);
    if(return_value != 1)
        return return_value;

    debug_print("%lu: recv lot %d\n", pthread_self(), nb);
    ret_total = 0;
    for(i = 0; i < nb; i++){
        if(channel->nb_elt <= 0)
            break;

        return_value = channel_recv_async(channel, (*datas) + channel->eltsize * i);

        if(return_value != 1)
            return return_value;

        ret_total++;
    }

    channel_signal_others(channel);

    return_value = channel_unlock(channel);
    if(return_value != 1)
        return  return_value;

    return ret_total;
}
