#ifndef MESSAGE_H
#define MESSAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client.h"

#define     TCP_CONN    2
#define     TCP_SUB     4
#define     TCP_UNSUB   5
#define     TCP_EXIT    6

#define     PAYLOAD_LEN 1500
#define     TOPIC_LEN   50

#define     INT         0
#define     SHORT_REAL  1
#define     FLOAT       2
#define     STRING      3

#define     SZ_INT      5
#define     SZ_SR       2
#define     SZ_FLOAT    6

#define     POS         0
#define     NEG         1

struct tcp_message
{
    int     type;
    bool    sf;
    char    cli_id[CLIENT_ID_LEN + 1];
    char    payload[TOPIC_LEN];
};

struct udp_message
{
    char    topic[TOPIC_LEN];
    uint8_t type;
    char    payload[PAYLOAD_LEN + 1];
};

struct message
{
    sockaddr_in addr;
    udp_message udp_msg;
};

#define     MIN_UDP_SIZE    53
#define     MIN_FWD_SIZE    (sizeof(struct sockaddr_in) + TOPIC_LEN + 1)
#define     TCP_MSG_SIZE    sizeof(struct tcp_message)
#define     UDP_MSG_SIZE    sizeof(struct udp_message) - 1
#define     MSG_SIZE        sizeof(struct message) - 1

std::ostream& operator<<(std::ostream& os, message& m);
int get_pkt_size(udp_message* udp_msg);

#endif
