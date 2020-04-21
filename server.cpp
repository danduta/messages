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
        DEBUG("Started server on port ");
        DEBUG(atoi(args[1]));
    }

    int port;
    fd_collection fds;
    fd_set tmp;
    int ret;
    vector<client> clients;
    map<string, vector<subscription>> subs;
    
    fds.udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(fds.udp_fd < 0, "socket");
    fds.tcp_fd = socket(PF_INET, SOCK_STREAM, 0);
    DIE(fds.tcp_fd < 0, "socket");

    fds.fdmax = (fds.udp_fd < fds.tcp_fd ? fds.tcp_fd : fds.udp_fd);

    struct sockaddr_in addr;
    port = atoi(args[1]);
    DIE(port < 0, "atoi");

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    ret = bind(fds.udp_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind");
    ret = bind(fds.tcp_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind");
    ret = listen(fds.tcp_fd, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    FD_ZERO(&fds.all_fds);
    FD_ZERO(&tmp);
    FD_SET(fds.tcp_fd, &fds.all_fds);
    FD_SET(fds.udp_fd, &fds.all_fds);

    DEBUG("Waiting for messages...")

    while (1) {
        tmp = fds.all_fds;
        int newfd = -1;

        ret = select(fds.fdmax + 1, &tmp, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        ret = handle_select(&tmp, &newfd, fds, &addr, clients, subs);
    }

    return 0;
}

int handle_select(
    fd_set* tmp,
    int* newfd,
    fd_collection &fds,
    struct sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs)
{
    char buffer[MSG_SIZE];
    memset(buffer, 0, MSG_SIZE);

    for (int i = 0; i <= fds.fdmax; i++) {
        if (FD_ISSET(i, tmp)) {
            if (i == fds.tcp_fd) {
                socklen_t len = sizeof(struct sockaddr_in);
                *newfd = accept(i, (struct sockaddr*)addr, &len);
                DIE(*newfd < 0, "accept");

                FD_SET(*newfd, &fds.all_fds);
                FD_SET(*newfd, &fds.tcp_clients);

                if (*newfd > fds.fdmax) {
                    fds.fdmax = *newfd;
                }

                memset(buffer, 0, MSG_SIZE);
                
                int bytes;
                bytes = recv(*newfd, buffer, MSG_SIZE, 0);

                tcp_message* msg = (tcp_message*)buffer;

                if (bytes != MSG_SIZE || msg->type != TCP_CONN) {
                    //TODO: create function that handles recv() calls
                    //client can send a large number of messages
                    //and they can get squished together
                    DEBUG("Invalid message from TCP client");
                    continue;
                }

                struct client c;

                c.fd = *newfd;
                strncpy(c.id, msg->cli_id, CLIENT_ID_LEN);

                cout << "New client (" << c.id << ") from " <<
                    inet_ntoa(addr->sin_addr) << endl;

                clients.emplace_back(c);
            } else if (i == fds.udp_fd) {

            } else {
                //FIXME: fd could be activated for any type of client
                //get client in else if condition, more fd_sets
                //TODO: struct containing all the fd_sets
                memset(buffer, 0, MSG_SIZE);

                int bytes;
                bytes = recv(i, buffer, MSG_SIZE, 0);
                DIE(bytes != MSG_SIZE, "recv");

                struct tcp_message* msg = (struct tcp_message*)buffer;

                vector<client>::iterator client;
                client = find_if(clients.begin(), clients.end(),
                    [msg] (const struct client &c) -> bool {
                        return strcmp(c.id, msg->cli_id) == 0;});

                if (client == clients.end() || clients.empty()) {
                    continue;
                }

                if (msg->type == TCP_SUB) {
                    DEBUG("Client..");
                    DEBUG(msg->cli_id);
                    DEBUG("..subscribed to topic..");
                    DEBUG(msg->payload);
                    auto it = subs.find(msg->payload);

                    if (it != subs.end()) {
                        subscription s;
                        s.client = &(*client);
                        s.sf = msg->sf;
                        it->second.emplace_back(s);
                    }
                } else if (msg->type == TCP_UNSUB) {
                    auto it = subs.find(msg->payload);

                    if (it != subs.end()) {
                        auto sub_to_remove = find_if(
                            it->second.begin(),
                            it->second.end(),
                            [client](const subscription &s) -> bool {
                                return s.client == &(*client);
                            });

                        if (sub_to_remove != it->second.end()) {
                            it->second.erase(sub_to_remove);
                        }
                    }
                }
            }
        }
    }

    return 0;
}