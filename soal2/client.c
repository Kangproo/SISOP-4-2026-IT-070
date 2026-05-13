#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9000
#define BUF_SIZE 4096

static int connect_to_server(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", host);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    int sock = connect_to_server(host, port);
    if (sock < 0) return 1;

    printf("Connected to DB Server on %s:%d\n", host, port);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    char input[BUF_SIZE];
    char response[BUF_SIZE + 1];

    while (1) {
        printf("db > ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\r\n")] = '\0';
        if (strcmp(input, "EXIT") == 0 || strcmp(input, "exit") == 0) {
            break;
        }

        strcat(input, "\n");

        if (send(sock, input, strlen(input), 0) < 0) {
            perror("send");
            break;
        }

        fd_set readfds;
        struct timeval tv;

        while (1) {
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 200000;

            int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
            if (ready < 0) {
                perror("select");
                close(sock);
                return 1;
            }
            if (ready == 0) {
                break;
            }

            ssize_t n = recv(sock, response, BUF_SIZE, 0);
            if (n < 0) {
                perror("recv");
                close(sock);
                return 1;
            }
            if (n == 0) {
                printf("Server closed connection\n");
                close(sock);
                return 0;
            }

            response[n] = '\0';
            printf("%s", response);
            if (response[n - 1] != '\n') printf("\n");
        }
    }

    close(sock);
    return 0;
}
