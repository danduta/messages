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
    struct sockaddr_in serv_addr;
    struct tcp_message m;

    serv_fd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(serv_fd < 0, "socket");

    port = atoi(args[3]);
    DIE (port < 0, "atoi");

    memset(&addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = PF_INET;
	serv_addr.sin_port = htons(port);
	ret = inet_aton(args[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(serv_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

    m.type = TCP_CONN;
    strncpy(m.payload, args[1], CLIENT_ID_LEN);

    ret = send(serv_fd, &m, sizeof(struct tcp_message), 0);
    DIE(ret != sizeof(struct tcp_message), "send");

    fd_set fds;
    fd_set tmp;

    FD_ZERO(&fds);
    FD_ZERO(&tmp);
    FD_SET(serv_fd, &fds);

    while (1) {

    }

    return 0;
}