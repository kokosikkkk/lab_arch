#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define HISTORY_FILE "chat_history.txt"

typedef struct{
    int sockfd;
    int echo;
    int flag;
    char inf[256];
}client_state;

void error(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}
int Socket(int domain, int type, int protocol){
    int res = socket(domain, type, protocol);
    if (res == -1) {
        perror("Ошибка при создании сокета.");
        exit(EXIT_FAILURE);
    }
    return res;
    
}
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    int res = bind(sockfd, addr, addrlen);
    if (res == -1){
        perror("Ошибка привязки. ");
        exit(EXIT_FAILURE);
    }
}
void Listen(int sockfd, int backlog){
    int res = listen(sockfd, backlog);
    if (res == -1){
        perror("Ошибка при прослушивании.");
        exit(EXIT_FAILURE);
    } 
}
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    int res = accept(sockfd, addr, addrlen);
    if (res == -1){
        perror("Ошибка соединения.");
        exit(EXIT_FAILURE);
    } return res;
}
void saveChat(const char *sender, const char *message) {
    FILE *file = fopen(HISTORY_FILE, "a");
    if (file != NULL) {
        fprintf(file, "%s: %s", sender, message);
        fclose(file);
    }
}
void showHistory(int sockfd) {
    char buffer[256];
    FILE *file = fopen(HISTORY_FILE, "r");
    
    if (file == NULL) {
        write(sockfd, "История пуста\n", 27);
        return;
    }
    write(sockfd, "История: \n", 8);
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        write(sockfd, buffer, strlen(buffer));
    }
    fclose(file);
}



int main(int argc, char *argv[]){
    if (argc < 2 ){
        fprintf(stderr, "Порт не указан. Программа завершена.\n");
        exit(EXIT_FAILURE);

    }
    int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1){
        error("Socket failed");
    }
    struct sockaddr_in serv_addr = {0};
    int port_number = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_number);
    Bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    Listen(server_fd, 5);
    printf("Сервер запущен.\n");
    printf("Если хотите остановить сервер введите Close.\n");
    
    client_state client = {0};
    client.echo = 1;
    client.flag = 0;
    while(1){
        socklen_t addrlen = sizeof(serv_addr);
        int newsockfd = Accept(server_fd, (struct sockaddr *) &serv_addr, &addrlen);
        printf("Клиент подключился, сокет %d. Ожидайте.\n", newsockfd);

        client.sockfd = newsockfd;
        client.echo = 1;
        client.flag = 0;
        snprintf(client.inf, sizeof(client.inf), "Клиент с номером сокета: %d", newsockfd);

        char buffer[256];
        int nread;
        int n;
    
        while(1){
            bzero(buffer, 256);
            nread = read(newsockfd, buffer, 255);
            if (nread == -1){
                error("Ошибка при чтении.\n");
            }
            if (nread == 0){
                printf("Клиент вышел.\n");
                break;
            }
            printf("Client %s: %s", client.inf, buffer);

            if (strncmp(buffer, "ping", 4) == 0) {
                bzero(buffer, 256);
                strcpy(buffer, "pong\n");
                n = write(newsockfd, buffer, strlen(buffer));
                if (n == -1){
                    error("Ошибка при записи\n");
                }
                printf("Отправлен ответ: pong\n");
                continue;  
            }
            if (strncmp(buffer, "History", 7) == 0) {
                showHistory(newsockfd);
                continue;
            }
            if (strncmp(buffer, "Echo", 4) == 0){
                client.echo = 1;
                client.flag = 0;
                bzero(buffer, 256);
                strcpy(buffer, "Эхо-сервер\n");
                n = write(newsockfd, buffer, strlen(buffer));
                if (n == -1){
                    error("Ошибка при записи.\n");
                }
                printf("Клиент в эхо.\n");
                continue;
            }
            if (strncmp(buffer, "Admin", 5) == 0){
                client.flag = 1;
                bzero(buffer, 256);
                strcpy(buffer, "Передано админу. Ожидайте.\n");
                n = write(newsockfd, buffer, strlen(buffer));
                if (n == -1){
                    error("Ошибка при записи.\n");
                }
                printf("Запрос на общение. Клиент: %s\n", client.inf);

                printf("Чтобы принять введите Yes, иначе No: ");

                char step[10];
                if (fgets(step, sizeof(step), stdin) != NULL){
                    if (strncmp(step, "Yes", 3) == 0){
                        client.echo = 0;
                        client.flag = 0;
                        bzero(buffer, 256);
                        write(newsockfd, "Запрос принят.\n", 14);
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи\n");
                        }
                        printf("Чат открыт.\n");
                        
                    }else {
                        client.flag = 0;
                        bzero(buffer, 256);
                        char *msg1 = "К сожалению, админ не принял запрос.\n";
                        write(newsockfd, msg1, strlen(msg1));
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи.\n");
                        }
                        printf("Запрос отклонен.\n");
                    }   
                }
                continue;
            }
            if (strstr(buffer, "Close") != NULL){
                printf("Завершение работы сервера.\n");
                close(newsockfd);
                close(server_fd);
                printf("Работа сервера прекращена. Чтобы снова работать с ним, занова запустите программу.\n");
                return 0;
            }
            int i = strncmp("Bye", buffer, 3);
            if ( i == 0){
                break;
            }
            
            if (client.echo){
                n = write(newsockfd, buffer, strlen(buffer));
                if (n == -1){
                    error("Ошибка.\n");
                }
                printf("%s", buffer);
            }
            else{
                saveChat("Client", buffer);
                printf("%s", buffer);

                while(!client.echo){
                    printf("Команды админа: 'Echo' - вернуть в эхо, 'Close' - завершить чат\n");
                    printf("Ввод сообщения: ");
                    bzero(buffer, 256);
                    if (fgets(buffer, 255, stdin) == NULL){
                        error("Ошибка при чтении ввода.\n");
                    }
                    
                    if (strncmp(buffer, "Echo", 4) == 0) {
                        client.echo = 1;
                        printf("Клиент возвращен в эхо.\n");
                        bzero(buffer, 256);
                        strcpy(buffer, "Админ вернул вас в эхо-сервер.\n");
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи.\n");
                        }
                        break;
                    }
                    
                    if (strncmp(buffer, "Close", 4) == 0) {
                        printf("Завершение чата с клиентом.\n");
                        bzero(buffer, 256);
                        strcpy(buffer, "Чат завершен админом.\n");
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи.\n");
                        }
                        client.echo = 1;
                        break;
                    }
                    
                    n = write(newsockfd, buffer, strlen(buffer));
                    if (n == -1){
                        error("Ошибка при записи.\n");
                    }

                    saveChat("Server", buffer);
                    
                    if (client.echo){
                        break;
                    }

                    bzero(buffer, 256);
                    nread = read(newsockfd, buffer, 255);

                    if (nread == -1){
                        error("Ошибка при чтении.\n");
                    }
                    if (nread == 0){
                        printf("Клиент отключился.\n");
                        break;
                    }
                    
                    if (strncmp(buffer, "Echo", 4) == 0){
                        client.echo = 1;
                        printf("Клиент вернулся в эхо.\n");
                        bzero(buffer, 256);
                        strcpy(buffer, "Эхо-сервер\n");
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи.\n");
                        }
                        break;
                    }
                    
                    if (strncmp(buffer, "History", 7) == 0) {
                        showHistory(newsockfd);
                        continue;
                    }
                    
                    if (strncmp(buffer, "ping", 4) == 0) {
                        bzero(buffer, 256);
                        strcpy(buffer, "pong\n");
                        n = write(newsockfd, buffer, strlen(buffer));
                        if (n == -1){
                            error("Ошибка при записи\n");
                        }
                        saveChat("Server", "pong\n");
                        continue;
                    }
                    
                    if (strncmp("Bye", buffer, 3) == 0){
                        break;
                    }
                    
                    printf("Client %s: %s", client.inf, buffer);
                    saveChat("Client", buffer);
                }
            }
        
        }
        close(newsockfd);
        printf("Клиент %s отключился.\n", client.inf);
    }
    close(server_fd); 
    return 0;
}