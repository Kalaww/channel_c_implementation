#ifndef CHANNEL_H
#define CHANNEL_H

struct channel;

/* flags */
#define CHANNEL_PROCESS_SHARED 1

struct channel *channel_create(int eltsize, int size, int flags);
void channel_destroy(struct channel *channel);
int channel_send(struct channel *channel, void *data);
int channel_close(struct channel *channel);
int channel_recv(struct channel *channel, void *data);

int channel_send_n(struct channel *channel, void **datas, int nb);
int channel_recv_n(struct channel *channel, void **datas, int nb);

#endif