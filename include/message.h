#ifndef MESSAGE_H
#define MESSAGE_H

#define     UDP_CONN    1
#define     TCP_CONN    2
#define     STDIN_COMM  3
#define     SUB         4
#define     UNSUB       5
#define     EXIT        6

#define     TCP_LEN     256

struct tcp_message
{
    int     type;
    char    payload[TCP_LEN];
};

#endif