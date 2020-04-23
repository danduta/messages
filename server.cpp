#include "client.h"
#include "server.h"
#include "message.h"

using namespace std;

int usage(int argc, char** args)
{
    if (argc < 2 || argc > 2) {
        cout << "Usage:\t./server <PORT>" << endl;
        return 1;
    } else {
        if (DEBUG_BUILD) {
            cout << "Warning: This binary is built in debug/verbose mode." << endl;
            cout << "To change this and avoid verbose messages change the value of" << endl;
            cout << "DEBUG_MODE in include/server.h to false\n" << endl;
        }
        DEBUG("Started server on port " + to_string(atoi(args[1])));
    }

    return 0;
}

int main(int argc, char** args)
{
    if (usage(argc, args))
        return -1;

    int port;
    fd_collection fds;
    fd_set tmp;
    int ret;
    vector<client> clients;
    map<string, vector<subscription>> subs;
    map<string, vector<message>> sf_msgs;

    
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

        ret = select(fds.fdmax + 1, &tmp, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        handle_select(fds, &tmp, &addr, clients, subs, sf_msgs);
    }

    return 0;
}

void handle_select(
    fd_collection &fds,
    fd_set* selected,
    sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs,
    map<string, vector<message>> &msgs)
{
    int newfd;
    //TODO: pass buffer as parameter to function to avoid
    // allocating 1500 bytes on the stack on every call
    char buffer[MSG_SIZE];
    memset(buffer, 0, MSG_SIZE);

    for (int i = 0; i <= fds.fdmax; i++) {
        if (FD_ISSET(i, selected)) {
            if (i == fds.tcp_fd) {
                socklen_t len = sizeof(sockaddr_in);
                newfd = accept(i, (sockaddr*)addr, &len);
                DIE(newfd < 0, "accept");

                FD_SET(newfd, &fds.all_fds);
                FD_SET(newfd, &fds.tcp_clients);

                if (newfd > fds.fdmax) {
                    fds.fdmax = newfd;
                }

                memset(buffer, 0, TCP_MSG_SIZE);
                
                ssize_t bytes = recv(newfd, buffer, MSG_SIZE, 0);
                tcp_message* msg = (tcp_message*)buffer;

                if (bytes != TCP_MSG_SIZE || msg->type != TCP_CONN) {
                    //TODO: create function that handles recv() calls
                    // multiple clients may connect at once
                    DEBUG("Invalid message from TCP client");
                    continue;
                }

                auto it = find_if(clients.begin(), clients.end(),
                    [msg] (const client &c) -> bool {
                        return strcmp(msg->cli_id, c.id) == 0;});

                if (it != clients.end() && it->status == INACTIVE) {
                    it->fd = newfd;
                    it->status = ACTIVE;

                    cout << "Client (" << it->id << ") from " <<
                        inet_ntoa(addr->sin_addr) << ":" <<
                        ntohs(addr->sin_port) << " reconnected." << endl;

                    auto old_msg_list = msgs.find(it->id);

                    if (old_msg_list == msgs.end()) {
                        continue;
                    }

                    for (auto old_msg : old_msg_list->second) {
                        DEBUG("Sending old messages...");
                        send(newfd, &old_msg, MSG_SIZE, 0);
                    }

                    continue;
                } else if (it != clients.end() && it->status == ACTIVE) {
                    continue;
                }

                client c;

                c.fd = newfd;
                c.status = ACTIVE;
                strncpy(c.id, msg->cli_id, CLIENT_ID_LEN);

                cout << "New client (" << c.id << ") from " <<
                    inet_ntoa(addr->sin_addr) << endl;

                clients.emplace_back(c);
            } else if (i == fds.udp_fd) {
                socklen_t len = sizeof(sockaddr_in);
                message* msg = (message*)buffer;
                udp_message* udp_msg = &msg->udp_msg;

                ssize_t bytes;
                bytes = recvfrom(i, udp_msg, UDP_MSG_SIZE, 0, (sockaddr*)&(msg->addr), &len);

                if (bytes < MIN_UDP_SIZE || bytes < 0) {
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
                        if (sub.client->status == INACTIVE &&
                            sub.sf == true) {
                            DEBUG("\ttoring message to forward later..");
                            auto cli_msg_list = msgs.find(sub.client->id);

                            if (cli_msg_list != msgs.end()) {
                                cli_msg_list->second.emplace_back(*msg);
                            } else {
                                vector<message> v;
                                v.emplace_back(*msg);
                                msgs.insert(pair<string, vector<message>>
                                    (sub.client->id, v));
                            }
                        }

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

                ssize_t bytes = recv(i, buffer, TCP_MSG_SIZE, 0);
                
                if (bytes != TCP_MSG_SIZE) {
                    continue;
                }

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
                    client->status = INACTIVE;
                    FD_CLR(client->fd, &fds.all_fds);
                    FD_CLR(client->fd, &fds.tcp_clients);
                    close(client->fd);

                    cout << "Client (" << client->id <<
                        ") disconnected." << endl;
                }
            }
        }
    }
}