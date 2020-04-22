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
    //TODO: use multimap???
    map<string, vector<subscription>> subs;
    
    fds.udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(fds.udp_fd < 0, "socket");
    fds.tcp_fd = socket(PF_INET, SOCK_STREAM, 0);
    DIE(fds.tcp_fd < 0, "socket");

    fds.fdmax = (fds.udp_fd < fds.tcp_fd ? fds.tcp_fd : fds.udp_fd);

    sockaddr_in addr;
    port = atoi(args[1]);
    DIE(port < 0, "atoi");

    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    ret = bind(fds.udp_fd, (sockaddr*)&addr, sizeof(sockaddr_in));
    DIE(ret < 0, "bind");
    ret = bind(fds.tcp_fd, (sockaddr*)&addr, sizeof(sockaddr_in));
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
    sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs)
{
    char buffer[MSG_SIZE];
    memset(buffer, 0, MSG_SIZE);

    for (int i = 0; i <= fds.fdmax; i++) {
        if (FD_ISSET(i, tmp)) {
            if (i == fds.tcp_fd) {
                socklen_t len = sizeof(sockaddr_in);
                *newfd = accept(i, (sockaddr*)addr, &len);
                DIE(*newfd < 0, "accept");

                FD_SET(*newfd, &fds.all_fds);
                FD_SET(*newfd, &fds.tcp_clients);

                if (*newfd > fds.fdmax) {
                    fds.fdmax = *newfd;
                }

                memset(buffer, 0, TCP_MSG_SIZE);
                
                int bytes;
                bytes = recv(*newfd, buffer, MSG_SIZE, 0);

                tcp_message* msg = (tcp_message*)buffer;

                if (bytes != TCP_MSG_SIZE || msg->type != TCP_CONN) {
                    //TODO: create function that handles recv() calls
                    //client can send a large number of messages
                    //and they can get squished together
                    DEBUG("Invalid message from TCP client");
                    continue;
                }

                client c;

                c.fd = *newfd;
                strncpy(c.id, msg->cli_id, CLIENT_ID_LEN);

                cout << "New client (" << c.id << ") from " <<
                    inet_ntoa(addr->sin_addr) << endl;

                clients.emplace_back(c);
            } else if (i == fds.udp_fd) {
                DEBUG("\nUDP message");

                socklen_t len = sizeof(sockaddr_in);
                message* msg = (message*)buffer;
                udp_message* udp_msg = &msg->udp_msg;

                ssize_t bytes;
                bytes = recvfrom(i, udp_msg, UDP_MSG_SIZE, 0, (sockaddr*)&(msg->addr), &len);
                DIE(bytes < 0, "recvfrom");

                DEBUG("\ttopic: " + string(udp_msg->topic));

                if (bytes < MIN_UDP_SIZE) {
                    DEBUG("Incomplete UDP packet.");
                    continue;
                }

                int fwd_size = MIN_FWD_SIZE;
                if (udp_msg->type == INT) {
                    fwd_size += SZ_INT;
                } else if (udp_msg->type == SHORT_REAL) {
                    fwd_size += SZ_SR;
                } else if (udp_msg->type == FLOAT) {
                    fwd_size += SZ_FLOAT;
                } else if (udp_msg->type == STRING) {
                    fwd_size +=
                        min(PAYLOAD_LEN, (int)strlen(udp_msg->payload));
                } else {
                    continue;
                }

                char topic[TOPIC_LEN + 1];
                strcpy(topic, udp_msg->topic);

                auto it = subs.find(topic);

                if (it != subs.end()) {
                    for (auto sub : it->second) {
                        DEBUG("Found subscriber with ID:" + string(sub.client->id));
                        //TODO: enable storing
                        bytes = send(sub.client->fd, msg, MSG_SIZE, 0);
                        if (bytes < 0) {
                            continue;
                        }

                        DEBUG("Sent " + to_string(bytes) + " bytes to " + string(sub.client->id));
                    }
                }
            } else {
                // sub/unsub/exit message from tcp client
                memset(buffer, 0, TCP_MSG_SIZE);

                int bytes;
                bytes = recv(i, buffer, TCP_MSG_SIZE, 0);
                DIE(bytes != TCP_MSG_SIZE, "recv");

                tcp_message* msg = (tcp_message*)buffer;

                auto client = find_if(clients.begin(), clients.end(),
                    [msg] (const struct client &c) -> bool {
                        return strcmp(c.id, msg->cli_id) == 0;});

                if (client == clients.end() || clients.empty()) {
                    continue;
                }

                if (msg->type == TCP_SUB) {
                    DEBUG("Client " + string(msg->cli_id) + " subscribed to topic " + string(msg->payload));
                    auto it = subs.find(msg->payload);

                    if (it != subs.end()) {
                        subscription s;
                        s.client = &(*client);
                        s.sf = msg->sf;
                        it->second.emplace_back(s);
                    } else {
                        vector<subscription> v;
                        subscription s;

                        s.client = &(*client);
                        s.sf = msg->sf;

                        v.emplace_back(s);
                        subs.insert(pair<string, vector<subscription>>
                            (msg->payload, v));
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
                } else if (msg->type == TCP_EXIT) {
                    //TODO: handle this case
                }
            }
        }
    }

    return 0;
}