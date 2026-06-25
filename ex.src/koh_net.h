#pragma once

#include <netinet/in.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

typedef enum NetStatus {
    NET_STAT_FIRST_UNUSED = 0,
    NET_STAT_GATHERED     = 1,
    NET_STAT_LAST_UNUSED  = 2,
} NetStatus;

typedef struct Net Net;

typedef struct Server_def {
    //struct timeval timeout;
    char     addr[32];
    uint32_t port;
} Server_def;

typedef struct Client_def {
    //struct timeval timeout;
    char     addr[32];
    uint32_t port;
} Client_def;

typedef enum Tag {
    TAG_UNUSED_FIRST = 0,
    TAG_INPUT        = 1,
    TAG_STRING       = 2,
    TAG_UNUSED_LAST  = 3,
} Tag;

typedef struct ClientEvent {
    uint8_t tag;
    void    *data;
} ClientEvent;

Net *net_new();
void net_free(Net *n);

void net_server_init(Net *n, Server_def def);
void net_client_init(Net *n, Client_def def);

bool net_server_gathered(Net *n);
//bool net_receive(Net *n, void *data, int len);
//bool net_send(Net *n, void *data, int len);

int net_clients_get_num(Net *n);
bool net_client_event_pop(Net *n, int client, ClientEvent *ev);
void net_client_event_push(Net *n, int client, void *data, int len);

void net_client_event_shutdown(ClientEvent *ev);

bool net_is_server(Net *n);
bool net_is_active(Net *n);
