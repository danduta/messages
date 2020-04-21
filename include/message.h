#ifndef MESSAGE_H
#define MESSAGE_H

#define     UDP_CONN    1
#define     TCP_CONN    2
#define     STDIN_COMM  3
#define     TCP_SUB     4
#define     TCP_UNSUB   5
#define     TCP_EXIT    6

#define     TCP_LEN     256

struct tcp_message
{
    int     type;                       // as defined above
    bool    sf;
    char    cli_id[CLIENT_ID_LEN + 1];
    char    payload[TCP_LEN];           // topic, most likely
};

#define     MSG_SIZE    sizeof(struct tcp_message)

#endif