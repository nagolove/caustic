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

typedef struct Net {
    // Команда которую читают клиенты или сервер в цикле
    _Atomic(uint32_t) command;
    // Состояние сервера
    _Atomic(uint32_t) status;
    thrd_t            worker;
    bool              is_server, is_active;
    // payload is Server* or Client* structure
    void              *payload;
    // Server_def or Client_def structure
    void              *def;
} Net;

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

void net_init(Net *n);
void net_shutdown(Net *n);

void net_server_init(Net *n, Server_def def);
void net_client_init(Net *n, Client_def def);

bool net_server_gathered(Net *n);
//bool net_receive(Net *n, void *data, int len);
//bool net_send(Net *n, void *data, int len);

int net_clients_get_num(Net *n);
bool net_client_event_pop(Net *n, int client, ClientEvent *ev);
void net_client_event_push(Net *n, int client, void *data, int len);

void net_client_event_shutdown(ClientEvent *ev);
