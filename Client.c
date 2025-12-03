#include <sys/types.h> //нужен для поддержки констант
#include <sys/socket.h> //в нем определена фенкция socket
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

void error(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}
int Socked(int domain, int type, int protocol){
    int res = socket(domain, type, protocol);
    if (res == -1){
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }return res;
}

void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    int res = connect(sockfd, addr, addrlen);
    if (res == -1){
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

}

int main(int argc, char *argv[]){
    if (argc < 3){
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 8080\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int fd = Socked(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in adr ={0};
    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL){
        fprintf(stderr, "Error: no such host");
        exit(EXIT_FAILURE);
    }

    adr.sin_family = AF_INET;
    adr.sin_port = htons(atoi(argv[2]));
    bcopy((char *)server->h_addr, (char *)&adr.sin_addr.s_addr, server->h_length);
    Connect(fd, (struct sockaddr *) &adr, sizeof (adr));

    printf("Connected to %s %s\n", argv[1], argv[2]);
    printf("=================================\n");
    printf("📢 Welcome to the chat room!\n");
    printf("📢 Step 1: Please set your username\n");
    printf("📢 Enter: /username your_name\n");
    printf("📢 Example: /username Ivan\n");
    printf("📢 After setting username, you can start chatting!\n");
    printf("=================================\n\n");
    printf("Available commands:\n");
    printf("  /username name - Set your username (required first)\n");
    printf("  Normal message - Direct input, will broadcast to everyone\n");
    printf("  ping           - Test server connection\n");
    printf("  Admin          - Request private chat with admin\n");
    printf("  Echo           - Return to broadcast chat mode\n");
    printf("  History        - View chat history\n");
    printf("  Bye            - Disconnect and exit\n");
    printf("\nTip: Each user has a different color for easy identification\n");

    char buffer[BUFFER_SIZE];
    int n;
    while (1){
        fflush(stdout);
        bzero(buffer, BUFFER_SIZE);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        select(fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(fd, &readfds)) {
            bzero(buffer, BUFFER_SIZE);
            n = read(fd, buffer, BUFFER_SIZE - 1);
            if (n > 0) {
                buffer[n] = '\0';
                printf("%s", buffer);
                fflush(stdout);
            }
            if (n == 0) {
                printf("Server disconnected\n");
                break;
            }
            if (n < 0){
                error("Error reading.");
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, BUFFER_SIZE - 1, stdin);
            n = write(fd, buffer, strlen(buffer));
            if (n < 0){
                error("Error writing");
            }
            int i = strncmp("Bye", buffer, 3);
            if (i == 0){
                break;
            }
        }
        if (strncmp("Bye", buffer, 3) == 0) {
            break;
        }
    }
    close(fd);
    return 0;
}