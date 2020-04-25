#include "include/utils.h"
#include "include/client.h"
#include "include/message.h"

#include <vector>
#include <map>
#include <algorithm>

using namespace std;

int server_usage(int argc, char** args)
{
    if (argc < 2 || argc > 2) {
        cout << "Usage:\t./server <PORT>" << endl;
        return 1;
    } else {
        if (DEBUG_BUILD) {
            cout << "Warning: This binary is built in ";
            cout << "debug/verbose mode." << endl;
            cout << "To change this and avoid verbose messages change ";
            cout << "the value of" << endl;
            cout << "DEBUG_MODE in include/server.h to false\n" << endl;
        }
        DEBUG("Started server on port " << atoi(args[1]) << endl);
    }

    return 0;
}

int handle_select(
    fd_collection &fds,
    fd_set* selected,
    sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs,
    map<string, vector<message>> &msgs);

int main(int argc, char** args)
{
    if (server_usage(argc, args))
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
    FD_SET(STDIN_FILENO, &fds.all_fds);

    DEBUG("Waiting for messages..." << endl);

    while (1) {
        tmp = fds.all_fds;

        ret = select(fds.fdmax + 1, &tmp, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        ret = handle_select(fds, &tmp, &addr, clients, subs, sf_msgs);
        if (ret < 0) {
            DEBUG("Closing server." << endl);
            return 0;
        }
    }

    return 0;
}

/*
 * Returns what find_if would return if the client
 * isnt' already subscribed. If it is, update
 * the SF flag of its subscription.
 */
vector<subscription>::iterator is_sub (
    char* id, vector<subscription> &list, bool sf)
{
    auto it = find_if(list.begin(), list.end(),
        [id] (const subscription &s) -> bool {
            return strcmp(s.client->id, id) == 0;
        });

    if (it != list.end() && it->sf != sf) {
        DEBUG("Updating SF flag of " + string(id) + " to " << boolalpha << sf);
        DEBUG(endl);
        it->sf = sf;
    }

    return it;
}

/*
 * Returns -1 in case of server shutdown or 0 on success, after
 * processing very socket that's selected. Handles incoming
 * connections on the TCP socket and forwards UDP messages
 * to clients.
 */
int handle_select(
    fd_collection &fds,
    fd_set* selected,
    sockaddr_in* addr,
    vector<client> &clients,
    map<string, vector<subscription>> &subs,
    map<string, vector<message>> &msgs)
{
    int newfd;
    /* 
     * TODO(danduta): pass buffer as parameter to function to avoid
     * allocating 1500 bytes on the stack on every call
     */
    char buffer[MSG_SIZE];
    memset(buffer, 0, MSG_SIZE);

    for (int i = 0; i <= fds.fdmax; i++) {
        if (FD_ISSET(i, selected)) {
            if (i == STDIN_FILENO) {
                fgets(buffer, 10, stdin);

                if (strncmp(buffer, "exit", 4) == 0) {
                    for (int fd = 0; fd <= fds.fdmax; fd++)
                        if (FD_ISSET(fd, &fds.tcp_clients))
                            close(fd);

                    close(fds.tcp_fd);
                    close(fds.udp_fd);
                    
                    return -1;
                }
            }
            if (i == fds.tcp_fd) {
                socklen_t len = sizeof(sockaddr_in);
                newfd = accept(i, (sockaddr*)addr, &len);
                DIE(newfd < 0, "accept");

                FD_SET(newfd, &fds.all_fds);
                FD_SET(newfd, &fds.tcp_clients);

                if (newfd > fds.fdmax)
                    fds.fdmax = newfd;

                memset(buffer, 0, TCP_MSG_SIZE);

                ssize_t bytes = recv(newfd, buffer, MSG_SIZE, 0);
                tcp_message* msg = (tcp_message*)buffer;

                if (bytes != TCP_MSG_SIZE || msg->type != TCP_CONN) {
                    DEBUG("Invalid message from TCP client" << endl);
                    continue;
                }
                // Look for the client in the client vector
                auto it = find_if(
                    clients.begin(),
                    clients.end(),
                    [msg] (const client &c) -> bool {
                        return strcmp(msg->cli_id, c.id) == 0;
                    });

                if (it != clients.end() && it->status == INACTIVE) {
                    // Update client status and fd in case of existing client
                    it->fd = newfd;
                    it->status = ACTIVE;

                    cout << "Client (" << it->id << ") from " <<
                        inet_ntoa(addr->sin_addr) << ":" <<
                        ntohs(addr->sin_port) << " reconnected." << endl;
                    /*
                     * Search for messages that accumulated while the
                     * client was inactive.
                     */
                    auto old_msg_list = msgs.find(it->id);

                    if (old_msg_list == msgs.end())
                        continue;

                    while (!old_msg_list->second.empty()) {
                        auto old_msg = old_msg_list->second.back();
                        DEBUG("Sending old messages to " << it->id << endl);

                        int fwd_size = get_pkt_size(&old_msg.udp_msg);
                        if (fwd_size < 0)
                            continue;

                        send(newfd, &old_msg, fwd_size, 0);

                        old_msg_list->second.pop_back();
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
                    inet_ntoa(addr->sin_addr) << ':' <<
                    ntohs(addr->sin_port) << endl;

                clients.emplace_back(c);
            } else if (i == fds.udp_fd) {
                socklen_t len = sizeof(sockaddr_in);
                message* msg = (message*)buffer;
                udp_message* udp_msg = &msg->udp_msg;

                ssize_t bytes;
                bytes = recvfrom(i, udp_msg, UDP_MSG_SIZE, 0,
                                (sockaddr*)&(msg->addr), &len);

                if (bytes < MIN_UDP_SIZE || bytes < 0) {
                    DEBUG("Incomplete UDP packet." << endl);
                    continue;
                }

                int fwd_size = get_pkt_size(udp_msg);
                if (fwd_size < 0)
                    continue;

                char topic[TOPIC_LEN + 1];
                strcpy(topic, udp_msg->topic);
                /*
                 * Look for the topic in the subscriptions map,
                 * iterator receives subs.end() if there are no subscribers
                 * (left) to the topic
                 */
                auto it = subs.find(topic);

                if (it != subs.end()) {
                    // Iterate through the subscribers
                    for (auto sub : it->second) {
                        DEBUG("Found subscriber with ID: " << sub.client->id << endl);
                        /*
                         * Store message for later forward if the client is 
                         * INACTIVE.
                         */
                        if (sub.client->status == INACTIVE &&
                            sub.sf == true) {
                            DEBUG("\tstoring message to forward later.." << endl);
                            auto cli_msg_list = msgs.find(sub.client->id);

                            // Insert a new vector if no subscribers yet
                            if (cli_msg_list != msgs.end()) {
                                cli_msg_list->second.emplace_back(*msg);
                            } else {
                                vector<message> v;
                                v.emplace_back(*msg);
                                msgs.insert(pair<string, vector<message>>
                                    (sub.client->id, v));
                            }
                            continue;
                        }

                        bytes = send(sub.client->fd, msg, fwd_size, 0);
                        if (bytes < 0) {
                            continue;
                        }

                        DEBUG("Sent " << bytes << " bytes to ");
                        DEBUG(sub.client->id << endl);
                    }
                }
            } else {
                memset(buffer, 0, TCP_MSG_SIZE);

                ssize_t bytes = recv(i, buffer, TCP_MSG_SIZE, 0);

                if (bytes != TCP_MSG_SIZE)
                    continue;

                tcp_message* msg = (tcp_message*)buffer;

                // Look for the client in the client vector
                auto client = find_if(
                    clients.begin(),
                    clients.end(),
                    [msg] (const struct client &c) -> bool {
                        return strcmp(c.id, msg->cli_id) == 0;
                    });

                if (client == clients.end() || clients.empty())
                    continue;

                if (msg->type == TCP_SUB) {
                    DEBUG(msg->cli_id << " is subbing to " << msg->payload << endl);
                    auto it = subs.find(msg->payload);
                    // Look for the topic that the client is trying to sub to
                    if (it != subs.end()) {
                        if (is_sub(msg->cli_id, it->second, msg->sf) !=
                            it->second.end()) {
                            DEBUG("Client is already subbed!" << endl);
                            continue;
                        }

                        subscription s;
                        s.client = &(*client);
                        s.sf = msg->sf;
                        // Add subscription to the list.
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
                        // Find the subscription and if there is one, erase it.
                        auto sub_to_remove = find_if(
                            it->second.begin(),
                            it->second.end(),
                            [client] (const subscription &s) -> bool {
                                return s.client == &(*client);
                            });

                        if (sub_to_remove != it->second.end())
                            it->second.erase(sub_to_remove);
                    }
                } else if (msg->type == TCP_EXIT) {
                    /*
                     * Close the socket and clear the file descriptor from
                     * the collection. Don't remove the client from
                     * the client list because it may reconnect
                     */
                    client->status = INACTIVE;
                    FD_CLR(client->fd, &fds.all_fds);
                    FD_CLR(client->fd, &fds.tcp_clients);
                    close(client->fd);

                    cout << "Client (" << client->id;
                    cout << ") disconnected." << endl;
                }
            }
        }
    }

    return 0;
}
