#ifndef CLIENT_H
#define CLIENT_H

using namespace std;

#include <list>
#include <string>

#define		CLIENT_ID_LEN   10
#define     ACTIVE          true
#define     INACTIVE        false

struct client
{
    int     fd;
    bool    status;
    char    id[CLIENT_ID_LEN + 1];
};

struct subscription
{
    struct client*	client;
    bool            sf;
};

#endif