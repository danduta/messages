#ifndef CLIENT_H
#define CLIENT_H

using namespace std;

#include <list>
#include <string>

#define		CLIENT_ID_LEN   10

struct client
{
    int     fd;    
    char    id[CLIENT_ID_LEN];
};

struct subscription
{
    struct client*	client;
    bool            sf;
};

#endif