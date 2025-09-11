// vim: fdm=marker
#include "koh_net.h"

#include "koh.h"
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <threads.h>
#include <unistd.h>
#include <sys/socket.h>

typedef struct Net {
    // Команда которую читают клиенты или сервер в цикле
    _Atomic(uint32_t)   command;
    // Состояние сервера
    _Atomic(uint32_t)   status;
    thrd_t              worker;
    bool                is_server, is_active;
    // payload is Server* or Client* structure
    void                *payload;
    // Server_def or Client_def structure
    void                *def;
    bool                is_freed;
} Net;

typedef enum ThreadCommand {
    THRD_CMD_NORMAL = 0,
    // Закончить цикл опроса клиентов сервером(покинуть игру)
    THRD_CMD_BREAK  = 1,
    // Ожидание присоединение клиентов
    //THRD_CMD_WAIT   = 1,
} ThreadCommand;

typedef struct Client {
    int  sock;
    bool connected;
} Client;

typedef struct Server {
    int    sock;
    int    *clients;
    // Количество подключенных клиентов, максимальной количество клиентов,
    // минимальное количество клиентов для создания игры.
    int    clientsnum, clientsmax, clientsmin;
    fd_set read_fds, write_fds;
} Server;

Net *net_new() {
    trace("net_new:\n");
    Net *n = calloc(1, sizeof(Net));
    assert(n);
    return n;
}

void net_free(Net *n) {
    trace("net_free:\n");
    assert(n);
    atomic_store(&n->command, THRD_CMD_BREAK);
    if (n->worker > 0) {
        int ret_code = 0;
        thrd_join(n->worker, &ret_code);
        printf("net worker joined with %d code\n", ret_code);
    }
    if (n->payload) {
        free(n->payload);
        n->payload = NULL;
    }
    if (n->def) {
        free(n->def);
        n->def = NULL;
    }
    free(n);
}

bool wait_for_clients(Net *net) {
    assert(net);
    Server *server = net->payload;
    assert(server);
    assert(server->clientsmin >= 0);
    assert(server->clientsmin <= server->clientsmax);

    struct timeval timeout = {
        .tv_sec = 0,
        //.tv_usec = 100,
        .tv_usec = 0,
    };

    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_size = sizeof(client_addr);

    bool shutdown = false;
    while (!shutdown) {
        printf("wait_for_clients\n");
        uint32_t command = atomic_load(&net->command);

        if (command == THRD_CMD_BREAK) {
            printf("wait_for_clients: break command\n");
            return false;
        }

        printf("before accept\n");
        int sock_client = accept(
                server->sock,
                (struct sockaddr*)&client_addr,
                &client_addr_size
                );
        printf("after accept\n");

        if (sock_client == -1) {
            printf("sock_client == -1\n");
            exit(1);
        }

        if (setsockopt(
                sock_client,
                SOL_SOCKET,
                SO_RCVTIMEO,
                (char*)&timeout,
                sizeof(timeout)
            )) {
            printf("setsockopt() receive timeout error\n");
        }

        if (setsockopt(
                sock_client,
                SOL_SOCKET,
                SO_SNDTIMEO,
                (char*)&timeout,
                sizeof(timeout)
            )) {
            printf("setsockopt() send timeout error\n");
        }

        server->clients[server->clientsnum++] = sock_client;
        if (server->clientsnum == server->clientsmax)
            break;
        if (server->clientsnum == server->clientsmin)
            break;
    }

    return true;
}

int server_worker(Net *n) {
    printf("server_worker\n");
    assert(n);
    assert(n->def);
    Server_def *def = n->def;
    Server *server = (Server*)n->payload;

    server->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in serv_addr = {0};

    printf("serving on %s:%u\n", def->addr, def->port);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(def->addr);
    serv_addr.sin_port = htons(def->port);

    int res = bind(
        server->sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)
    );

    if (res) {
        char buf[200] = {0};
        snprintf(buf, sizeof(buf),"bind() failed with '%s'", strerror(errno));
        exit(1);
    }

    // TODO backlog = 20
    int backlog = 0;
    listen(server->sock, backlog);

    server->clientsnum = 0;
    server->clientsmax = 8;
    server->clients = calloc(server->clientsmax, sizeof(server->clients[0]));

    server->clientsmin = 1;

    bool shutdown = false;

    if (!wait_for_clients(n)) {
        shutdown = true;
    }

    n->status = NET_STAT_GATHERED;
    printf("clients num %d\n", server->clientsnum);

    FD_ZERO(&server->read_fds);
    FD_ZERO(&server->write_fds);

    int max_fd = -1;
    for (int j = 0; j < server->clientsnum; j++) {
        if (server->clients[j] > max_fd) 
            max_fd = server->clients[j];
    }

    while (!shutdown) {
        uint32_t command = atomic_load(&n->command);

        switch (command) {
            case THRD_CMD_BREAK:
                printf("server_worker: break command\n");
                shutdown = true;
                break;
            case THRD_CMD_NORMAL:
                usleep(1000);
                break;
        }

        struct timeval timeout = {
            .tv_sec  = 0,
            .tv_usec = 10000,
        };
        int ready = select(
            max_fd + 1, &server->read_fds, &server->write_fds, NULL, &timeout
        );

        if (ready == -1) {
            // error handling
        }

        /*
        // {{{

        int bufsize = 255;
        char buf[bufsize];

        memset(buf, 0, sizeof(buf[0]) * bufsize);
        int bytesread = recv(sock_client, buf, bufsize, 0);

        if (bytesread == -1) {
            char buf[200] = {0};
            sprintf(buf, "recv() failed with '%s'", strerror(errno));
            perror(buf);
        }

        printf("bytesread %d\n", bytesread);

        if (bytesread > 1) {
            char *p = buf;

            if (*p == TAG_STRING) {
                p++;
                size_t len = *(size_t*)p;
                p += sizeof(size_t);

                printf("len %zu\n", len);

                char str[255] = {0};
                memcpy(str, p, len);
                printf("str %s\n", str);

                close(sock_client);
            }
        }

        */

        /*
        memset(buf, 0, sizeof(buf[0]) * bufsize);
        int bytessend = send(sock_client, buf, 1, 0);

        if (bytessend == -1) {
            char buf[200] = {0};
            sprintf(buf, "send() failed with '%s'", strerror(errno));
            perror(buf);
        }
        */

        // }}}

    }

    for (int j = 0; j < server->clientsnum; ++j) {
        close(server->clients[j]);
    }
    printf("clients sockets were closed\n");
    free(server->clients);
    close(server->sock);
    memset(server, 0, sizeof(*server));
    return 0;
}

int client_worker(Net *n) {
    printf("client_worker\n");
    assert(n);
    assert(n->def);
    Client_def *def = n->def;
    Client *client = (Client*)n->payload;

    printf("connection to %s:%u\n", def->addr, def->port);

    bool shutdown = false;
    
    client->sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;  
    serv_addr.sin_addr.s_addr = inet_addr(def->addr);  
    serv_addr.sin_port = htons(def->port); 


    while (!shutdown) {
        uint32_t command = atomic_load(&n->command);

        if (command == THRD_CMD_BREAK) 
            shutdown = true;

        // XXX: Сколько происходит ожидание?
        int retcode = connect(
            client->sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)
        );

        if (retcode == -1) {
            char buf[200] = {0};
            snprintf(
                buf, sizeof(buf), "connect() failed with '%s'", strerror(errno)
            );
            printf("%s\n", buf);
            exit(1);
        } else if (retcode == 0)
            break;

    }

    while (!shutdown) {
        printf("client_worker: loop\n");
        uint32_t command = atomic_load(&n->command);

        switch (command) {
            case THRD_CMD_BREAK:
                shutdown = true;
                break;
            case THRD_CMD_NORMAL:
                usleep(1000);
                break;
        }
    }

    return 0;
}

void net_server_init(Net *n, Server_def def) {
    assert(n);

    n->def = calloc(1, sizeof(def));
    memcpy(n->def, &def, sizeof(def));

    n->is_server = true;
    n->command = THRD_CMD_NORMAL;
    n->payload = calloc(1, sizeof(Server));

    thrd_create(&n->worker, (int(*)(void*))server_worker, n);
    printf("n->worker = %p\n", (void*)n->worker);
}

void net_client_init(Net *n, Client_def def) {
    assert(n);

    n->def = calloc(1, sizeof(def));
    memcpy(n->def, &def, sizeof(def));

    n->is_server = false;
    n->command = THRD_CMD_NORMAL;
    n->payload = calloc(1, sizeof(Client));

    thrd_create(&n->worker, (int(*)(void*))client_worker, n);
    /*printf("n->worker = %zu\n", n->worker);*/
    printf("n->worker = %p\n", (void*)n->worker);
}

bool net_receive(Net *n, void *data, int len) {
    assert(n);
    return false;
}

bool net_send(Net *n, void *data, int len) {
    return false;
}

int net_clients_get_num(Net *n) {
    return 0;
}

bool net_client_event_pop(Net *n, int client, ClientEvent *ev) {
    return false;
}

void net_client_event_push(Net *n, int client, void *data, int len) {
}

bool net_server_gathered(Net *n) {
    assert(n);
    return n->status == NET_STAT_GATHERED;
}

void net_client_event_shutdown(ClientEvent *ev) {
}

bool net_is_server(Net *n) {
    assert(n);
    return n->is_server;
}

bool net_is_active(Net *n) {
    assert(n);
    return n->is_active;
}

