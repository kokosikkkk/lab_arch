#include <sys/types.h> //нужен для поддержки констант
#include <sys/socket.h> //в нем определена фенкция socket
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h> 
#include <netdb.h>


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
        exit(EXIT_FAILURE);
    }
    int fd = Socked(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in adr ={0};
    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL){
        fprintf(stderr, "Ошибка нет такого хоста");
        exit(EXIT_FAILURE);
    }

    adr.sin_family = AF_INET;
    adr.sin_port = htons(atoi(argv[2]));
    bcopy((char *)server->h_addr, (char *)&adr.sin_addr.s_addr, server->h_length);
    Connect(fd, (struct sockaddr *) &adr, sizeof (adr));
    printf("Вы подключились к %s %s\n", argv[1], argv[2]);
    printf("Введите ping, чтобы удостоверится подключились вы к серверу или нет.\n");
    printf("Если хотите отключиться от сервера введите Bye\n");
    printf("Если хотите связать с админом, введите Admin\n");
    printf("Если хотите вернуться в эхо-сервер, введите Echo\n");
    printf("Если хотите посмотреть историю чата с сервером, введите History\n");
    char buffer[256];
    int n;
    while (1){
        fflush(stdout);
        bzero(buffer, 256);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        select(fd + 1, &readfds, NULL, NULL, NULL);
        
        if (FD_ISSET(fd, &readfds)) {
            bzero(buffer, 256);
            n = read(fd, buffer, 255);
            if (n > 0) {
                printf("Сервер: %s", buffer);
            }
            if (n == 0) {
                printf("Сервер отключился\n");
                break;
            }
            if (n < 0){
                error("Ошибка при чтении.");
            }
        }
    
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, 255, stdin);
            n = write(fd, buffer, strlen(buffer));
            if (n < 0){
                error("Ошибка при записи");
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