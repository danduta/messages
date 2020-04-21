#include "client.h"
#include "server.h"
#include "message.h"

using namespace std;

int main(int argc, char** args)
{
    if (argc < 2 || argc > 2) {
        cout << "Usage:\t./server <PORT>" << endl;
        return -1;
    } else {
        // cout << "Started server on port " << atoi(args[1]) << endl;
        DEBUG("Started server on port ");
        DEBUG(atoi(args[1]));
    }

    int port;
    int tcp, udp;
    int ret, fdmax;
    vector<client> clients;
    // map of topics and its associated clients
    map <string, list<subscription>> subs;
    
    udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp < 0, "socket");
    tcp = socket(PF_INET, SOCK_STREAM, 0);
    DIE(tcp < 0, "socket");

    fdmax = (udp < tcp ? tcp : udp);

    fd_set fds;
    fd_set tmp;
    
    struct sockaddr_in addr;
    port = htons(atoi(args[1]));
    DIE(port < 0, "atoi");

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    ret = bind(udp, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind");
    ret = bind(tcp, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind");
    ret = listen(tcp, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    FD_ZERO(&fds);
    FD_ZERO(&tmp);
    FD_SET(tcp, &fds);
    FD_SET(udp, &fds);

    // cout << "Waiting for messages..." << endl;
    DEBUG("Waiting for messages...")

    while (1) {
        tmp = fds;
        int newfd = -1;

        ret = select(fdmax + 1, &tmp, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        ret = handle_select(&fds, &tmp, &newfd, &fdmax, tcp, udp, &addr, clients);
    }

    return 0;
}

int handle_select(
    fd_set* set,
    fd_set* tmp,
    int* newfd,
    int* fdmax,
    int tcp_fd,
    int udp_fd,
    struct sockaddr_in* addr,
    vector<client> &clients)
{
    for (int i = 0; i <= *fdmax; i++) {
        if (FD_ISSET(i, tmp)) {
            if (i == tcp_fd) {
                socklen_t len = sizeof(struct sockaddr_in);
                *newfd = accept(i, (struct sockaddr*)addr, &len);
                DIE(*newfd < 0, "accept");

                FD_SET(*newfd, set);

                if (*newfd > *fdmax) {
                    *fdmax = *newfd;
                }

                char buffer[TCP_LEN];
                int bytes;

                bytes = recv(*newfd, buffer, TCP_LEN, 0);

                struct tcp_message* msg = (struct tcp_message*)buffer;

                if (bytes != sizeof(struct tcp_message) ||
                    msg->type != TCP_CONN) {
                    continue;
                }

                struct client c;

                c.fd = *newfd;
                strncpy(c.id, msg->payload, CLIENT_ID_LEN);

                cout << "New client (" << c.id << ") from " <<
                    inet_ntoa(addr->sin_addr) << endl;

                clients.emplace_back(c);
            }
        }
    }

    return 0;
}