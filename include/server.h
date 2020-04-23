#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client.h"
#include "message.h"

#include <algorithm>
#include <vector>
#include <map>

#define     DEBUG_BUILD true    // change in production haha

#define     MAX_CLIENTS 20      // max connections waiting in queue

struct fd_collection {
    int     tcp_fd;
    int     udp_fd;
    fd_set  all_fds;
    fd_set  tcp_clients;

    int     fdmax;
};

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0);

#define DEBUG(msg)                          \
    do {                                    \
        if(DEBUG_BUILD) {                   \
            std::cout << "[DEBUG] ";        \
            std::cout << msg << std::endl;  \
        }                                   \
    } while (0);                            \

void handle_select(
    fd_collection &fds,
    fd_set* selected,
    struct sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs,
    map<string, vector<message>> &messages);

#endif