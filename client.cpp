#include "client.h"
#include "server.h"
#include "message.h"

using namespace std;

int main(int argc, char** args)
{
    if (argc < 4 || argc > 4) {
        cout << "Usage:\t./subscriber <ID_Client> <IP_Server> <Port_Server>" << endl;
        return -1;
    }

    int port;
    int serv_fd;
    int ret;
    char* id = args[1];
    struct sockaddr_in serv_addr;
    struct tcp_message m;

    serv_fd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(serv_fd < 0, "socket");

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

    ret = send(serv_fd, &m, MSG_SIZE, 0);
    DIE(ret != MSG_SIZE, "send");
    cout << "Sizeof msg: " << MSG_SIZE << endl;
    cout << "Sent " << ret << " bytes" << endl;

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
                    char buffer[TCP_LEN];
                    fgets(buffer, TCP_LEN, stdin);
                    char* newline = strchr(buffer, '\n');

                    if (newline) {
                        *newline = '\0';
                    }

                    DEBUG("\nRead from stdin:");
                    DEBUG(buffer);

                    if (strncmp(buffer, "exit", 4) == 0) {
                        m.type = TCP_EXIT;
                    } else if (strncmp(buffer, "subscribe", 9) == 0) {
                        DEBUG("Subscribing...");
                        strcpy(m.payload, strchr(buffer, ' ') + 1);
                        m.type = TCP_SUB;
                    } else if (strncmp(buffer, "unsubscribe", 11) == 0) {
                        DEBUG("Unsubscribing...");
                        strcpy(m.payload, strchr(buffer, ' ') + 1);
                        m.type = TCP_UNSUB;
                    } else {
                        continue;
                    }

                    strncpy(m.cli_id, id, CLIENT_ID_LEN);
                    m.sf = false;
                    //TODO: fix getting sf from input buffer
                    ret = send(serv_fd, &m, MSG_SIZE, 0);
                    DIE(ret != MSG_SIZE, "send");

                    DEBUG("Sent message:");
                    DEBUG(m.payload);
                }
            }
        }
    }

    return 0;
}