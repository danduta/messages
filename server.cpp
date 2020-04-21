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
    // fd_collection fds;
    int tcp, udp;
    int ret, fdmax;
    vector<client> clients;
    map<string, vector<subscription>> subs;
    
    udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp < 0, "socket");
    tcp = socket(PF_INET, SOCK_STREAM, 0);
    DIE(tcp < 0, "socket");

    fdmax = (udp < tcp ? tcp : udp);

    fd_set fds;
    fd_set tmp;
    
    struct sockaddr_in addr;
    port = atoi(args[1]);
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

        ret = handle_select(&fds, &tmp, &newfd, &fdmax, tcp, udp, &addr, clients, subs);
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
    vector<client> &clients,
    map<string, vector<subscription>> &subs)
{

    char buffer[MSG_SIZE];
    memset(buffer, 0, MSG_SIZE);

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

                memset(buffer, 0, MSG_SIZE);
                
                int bytes;
                bytes = recv(*newfd, buffer, MSG_SIZE, 0);

                struct tcp_message* msg = (struct tcp_message*)buffer;

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
            } else if (i == udp_fd) {

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