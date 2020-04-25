#include "include/client.h"
#include "include/utils.h"
#include "include/message.h"

using namespace std;

int client_usage(int argc, char** args)
{
    if (argc < 4 || argc > 4) {
        cout << "Usage:\t./subscriber ";
        cout << "<ID_Client> <IP_Server> <Port_Server>" << endl;
        return 1;
    } else {
        if (DEBUG_BUILD) {
            cout << "Warning: This binary is built in ";
            cout << "debug/verbose mode." << endl;
            cout << "To change this and avoid verbose messages change ";
            cout << "the value of" << endl;
            cout << "DEBUG_MODE in include/server.h to false\n" << endl;
        }
    }

    return 0;
}

#define OUT 0
#define IN  1

bool is_input_valid(char* input)
{
    if (strncmp(input, "exit", 4) == 0) {
        return true;
    }

    if (strlen(input) <= strlen("exit\n")) {
        return false;
    }

    int state = OUT; 
    unsigned wc = 0;

    char* p = input;

    while (*p)
    {
        if (*p == ' ' || *p == '\t') {
            state = OUT;
        }
        else if (state == OUT)
        {
            state = IN;
            ++wc;
        }

        ++p;
    }

    if (wc != 3) {
        return false;
    }

    char *newline = strchr(input, '\n');

    if (*(newline - 1) != '0' && *(newline - 1) != '1') {
        return false;
    }

    *newline = '\0';

    return true;
}

int main(int argc, char** args)
{
    if (client_usage(argc, args)) {
        return -1;
    }

    int port;
    int serv_fd;
    int ret;
    char* id = args[1];
    sockaddr_in serv_addr;
    tcp_message m;
    message rcv_msg;

    serv_fd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(serv_fd < 0, "socket");

    int yes = 1;
    ret = setsockopt(serv_fd, IPPROTO_TCP, TCP_NODELAY,
                    (char*)&yes, sizeof(int));
    DIE (ret < 0, "setsockopt");

    port = atoi(args[3]);
    DIE (port < 0, "atoi");

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = PF_INET;
	serv_addr.sin_port = htons(port);
	ret = inet_aton(args[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(serv_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

    m.type = TCP_CONN;
    strncpy(m.cli_id, id, CLIENT_ID_LEN);

    do {
        ret = send(serv_fd, &m, TCP_MSG_SIZE, 0);
    } while (ret != TCP_MSG_SIZE);

    fd_set fds;
    fd_set tmp;

    FD_ZERO(&fds);
    FD_ZERO(&tmp);
    FD_SET(serv_fd, &fds);
    FD_SET(STDIN_FILENO, &fds);

    int fdmax = serv_fd;

    while (1) {
        tmp = fds;

        ret = select(fdmax + 1, &tmp, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp)) {
                if (i == STDIN_FILENO) {
                    char buffer[2 * TOPIC_LEN];
                    fgets(buffer, 2 * TOPIC_LEN, stdin);

                    if (!is_input_valid(buffer)) {
                        DEBUG("Invalid input!");
                        DEBUG_END();
                        continue;
                    }

                    m.sf = false;

                    if (strncmp(buffer, "exit", 4) == 0) {
                        m.type = TCP_EXIT;
                    } else if (strncmp(buffer, "subscribe", 9) == 0) {
                        DEBUG("Subscribing...");
                        DEBUG_END();

                        if (buffer[strlen(buffer) - 1] == '1') {
                            m.sf = true;
                        }

                        strcpy(m.payload, strchr(buffer, ' ') + 1);

                        char* space = strchr(m.payload, ' ');

                        if (space) {
                            *space = '\0';
                        }

                        m.type = TCP_SUB;
                    } else if (strncmp(buffer, "unsubscribe", 11) == 0) {
                        DEBUG("Unsubscribing...");
                        DEBUG_END();
                        strcpy(m.payload, strchr(buffer, ' ') + 1);
                        m.type = TCP_UNSUB;
                    } else {
                        continue;
                    }

                    strncpy(m.cli_id, id, CLIENT_ID_LEN);

                    do {
                        ret = send(serv_fd, &m, TCP_MSG_SIZE, 0);
                    } while (ret != TCP_MSG_SIZE);

                    if (m.type == TCP_UNSUB) {
                        cout << "un";
                    }
                    cout << "subscribed " << m.payload;
                    if (m.sf == true) {
                        cout << " with sf";
                    }
                    cout << endl;

                    DEBUG("Sent message: " + string(m.payload));
                    DEBUG_END();

                    if (m.type == TCP_EXIT) {
                        DEBUG("Closing client...");
                        DEBUG_END();

                        close(serv_fd);
                        return 0;
                    }
                } else if (i == serv_fd) {
                    ssize_t bytes = recv(serv_fd, &rcv_msg, MSG_SIZE, 0);

                    if (bytes == 0) {
                        DEBUG("Server is closing the connection. Closing sockets");
                        DEBUG_END();

                        close(serv_fd);
                        return 0;
                    } else if (bytes < 0 || bytes < MIN_FWD_SIZE) {
                        continue;
                    }
                    /*
                     * TODO(danduta): TCP buffer can contain multiple
                     * glued messages?
                     */

                    cout << rcv_msg << endl;
                }
            }
        }
    }

    return 0;
}