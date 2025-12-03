#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>  // for select
#include <time.h>        // for time stamp

#define HISTORY_FILE "chat_history.txt"

// for Chat room
#define MAX_CLIENTS 10
#define USERNAME_LEN 32
#define BUFFER_SIZE 512

#define MAX_COLORS 8
typedef enum {
    COLOR_RED = 31,
    COLOR_GREEN = 32,
    COLOR_YELLOW = 33,
    COLOR_BLUE = 34,
    COLOR_MAGENTA = 35,
    COLOR_CYAN = 36,
    COLOR_WHITE = 37,
    COLOR_BRIGHT_RED = 91
} ColorCode;


ColorCode available_colors[MAX_COLORS] = {
    COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE,
    COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE, COLOR_BRIGHT_RED
};

// changed: add client state structure
typedef struct {
    int sockfd;
    int echo;
    int admin_chatting;  // Added: Whether chatting with admin +++
    char inf[256];
    char username[USERNAME_LEN];  // Client username
    int has_username;    // Username set flag
    ColorCode color;
} client_state;


client_state clients[MAX_CLIENTS];
int client_count = 0;               // Current online client count

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int Socket(int domain, int type, int protocol) {
    int res = socket(domain, type, protocol);
    if (res == -1) {
        perror("Ошибка при создании сокета.");
        exit(EXIT_FAILURE);
    }
    return res;
}

void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int res = bind(sockfd, addr, addrlen);
    if (res == -1) {
        perror("Ошибка привязки. ");
        exit(EXIT_FAILURE);
    }
}

void Listen(int sockfd, int backlog) {
    int res = listen(sockfd, backlog);
    if (res == -1) {
        perror("Ошибка при прослушивании.");
        exit(EXIT_FAILURE);
    }
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int res = accept(sockfd, addr, addrlen);
    if (res == -1) {
        perror("Ошибка соединения.");
        exit(EXIT_FAILURE);
    }
    return res;
}

void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%H:%M:%S", tm_info);
}

// add:color assign function
ColorCode assign_color() {
    static int color_index = 0;
    ColorCode color = available_colors[color_index];
    color_index = (color_index + 1) % MAX_COLORS;
    return color;
}

// change: Save chat history
void saveChat(const char *sender, const char *message) {
    FILE *file = fopen(HISTORY_FILE, "a");
    if (file != NULL) {
        char timestamp[20];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(file, "[%s] %s: %s", timestamp, sender, message);
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

// change: Broadcast message function
void broadcast_to_all(int sender_fd, client_state *sender, char *message) {
    char broadcast_msg[BUFFER_SIZE * 2];
    char timestamp[20];

    get_timestamp(timestamp, sizeof(timestamp));

    // add ANSI colror
    if (sender->has_username && strlen(sender->username) > 0) {
        snprintf(broadcast_msg, sizeof(broadcast_msg),
                "[%s] \033[1;%dm[%s]\033[0m: %s",  // username have color
                timestamp, sender->color, sender->username, message);
    } else {
        snprintf(broadcast_msg, sizeof(broadcast_msg),
                "[%s] \033[1;%dm[Client%d]\033[0m: %s",
                timestamp, sender->color, sender_fd, message);
    }

    if (sender->has_username) {
        saveChat(sender->username, message);
    } else {
        saveChat(sender->inf, message);
    }

    // Broadcast to all other clients in chat mode
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 &&
            clients[i].sockfd != sender_fd &&
            clients[i].echo == 1) {
            write(clients[i].sockfd, broadcast_msg, strlen(broadcast_msg));
        }
    }
}

// Added: Function to find client index
int find_client_index(int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == sockfd) {
            return i;
        }
    }
    return -1;
}

// add: Set username function
void set_username(int client_index, char *input_buffer) {
    client_state *client = &clients[client_index];

    char *command = strtok(input_buffer, " \n");
    char *new_name = strtok(NULL, " \n");

    if (new_name == NULL || strlen(new_name) == 0) {
        write(client->sockfd, "Error: Please provide username, format: /username yourname\n", 70);
        return;
    }

    if (strlen(new_name) >= USERNAME_LEN) {
        write(client->sockfd, "Error: Username too long\n", 30);
        return;
    }

    // Save username
    strncpy(client->username, new_name, USERNAME_LEN - 1);
    client->username[USERNAME_LEN - 1] = '\0';
    client->has_username = 1;

    // Send confirmation
    char confirm_msg[100];
    snprintf(confirm_msg, sizeof(confirm_msg), "Username set to: %s\n", client->username);
    write(client->sockfd, confirm_msg, strlen(confirm_msg));

    // welcome message
    char broadcast_msg[150];
    char timestamp[20];
    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(broadcast_msg, sizeof(broadcast_msg),
            "[%s] \033[1;33m[System]\033[0m: Welcome new user \033[1;%dm%s\033[0m to the chat room!\n",
            timestamp, client->color, client->username);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].sockfd != client->sockfd && clients[i].echo == 1) {
            write(clients[i].sockfd, broadcast_msg, strlen(broadcast_msg));
        }
    }
}

// Add: Main function to handle client messages
void handle_client_message(int client_index, char *buffer) {
    client_state *client = &clients[client_index];
    int sockfd = client->sockfd;

    // username setting command
    if (strncmp(buffer, "/username", 9) == 0) {
        set_username(client_index, buffer);
        return;
    }

    // Check if username is set
    if (!client->has_username && client->echo == 1) {
        write(sockfd, "Please set username first! Enter: /username yourname\n", 60);
        return;
    }

    printf("Client %d (%s): %s", sockfd,
           client->has_username ? client->username : "Unnamed",
           buffer);

    // Handle special commands
    if (strncmp(buffer, "ping", 4) == 0) {
        write(sockfd, "pong\n", 5);
        return;
    }

    if (strncmp(buffer, "History", 7) == 0) {
        showHistory(sockfd);
        return;
    }

    if (strncmp(buffer, "Echo", 4) == 0) {
        client->echo = 1;
        client->admin_chatting = 0;
        write(sockfd, "Switched to broadcast chat mode\n", 40);
        return;
    }

    if (strncmp(buffer, "Admin", 5) == 0) {
        client->admin_chatting = 1;
        client->echo = 0;
        write(sockfd, "Переключено в приватный чат с администратором (введите Echo для возврата)\n", 70);
        return;
    }

    if (strncmp(buffer, "Bye", 3) == 0) {
        // User exit broadcast
        if (client->has_username) {
            char leave_msg[150];
            char timestamp[20];
            get_timestamp(timestamp, sizeof(timestamp));
            snprintf(leave_msg, sizeof(leave_msg),
                    "[%s] \033[1;33m[System]\033[0m: User \033[1;%dm%s\033[0m left the chat\n",
                    timestamp, client->color, client->username);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].sockfd > 0 && clients[i].sockfd != sockfd && clients[i].echo == 1) {
                    write(clients[i].sockfd, leave_msg, strlen(leave_msg));
                }
            }
        }
        return;
    }

    if (strstr(buffer, "Close") != NULL) {
        // Server shutdown will be handled in main function
        return;
    }

    // Normal message processing
    if (client->echo == 1) {
        // Broadcast chat mode: send to everyone
        broadcast_to_all(sockfd, client, buffer);

        // Echo back to sender
        char echo_msg[BUFFER_SIZE];
        char timestamp[20];
        get_timestamp(timestamp, sizeof(timestamp));

        if (client->has_username) {
            snprintf(echo_msg, sizeof(echo_msg),
                    "[%s] \033[1;90m[You]\033[0m: %s", timestamp, buffer);  // grey for self
        } else {
            snprintf(echo_msg, sizeof(echo_msg),
                    "[%s] \033[1;90m[You]\033[0m: %s", timestamp, buffer);
        }
        write(sockfd, echo_msg, strlen(echo_msg));
    } else if (client->admin_chatting == 1) {
        // Admin mode: simplified, just echo
        write(sockfd, "[Admin mode] Message recorded\n", 40);
        saveChat("AdminChat", buffer);
    }
}

// Add: Function to init client state
void init_client_state(int client_index, int sockfd) {
    client_state *client = &clients[client_index];

    client->sockfd = sockfd;
    client->echo = 1;
    client->admin_chatting = 0;
    client->has_username = 0;
    memset(client->username, 0, USERNAME_LEN);
    snprintf(client->inf, sizeof(client->inf), "Client%d", sockfd);
    client->color = assign_color();  // give color to new client

    // Send welcome message
    char welcome_msg[BUFFER_SIZE];
    strcpy(welcome_msg, "=================================\n");
    write(sockfd, welcome_msg, strlen(welcome_msg));
    strcpy(welcome_msg, "Добро пожаловать в чат!\n");
    write(sockfd, welcome_msg, strlen(welcome_msg));
    strcpy(welcome_msg, "Сначала установите имя пользователя: /username ваше_имя\n");
    write(sockfd, welcome_msg, strlen(welcome_msg));
    strcpy(welcome_msg, "После установки имени можно начинать общаться!\n");
    write(sockfd, welcome_msg, strlen(welcome_msg));
    strcpy(welcome_msg, "=================================\n");
    write(sockfd, welcome_msg, strlen(welcome_msg));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <порт>\n", argv[0]);
        fprintf(stderr, "Пример: %s 8080\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initi all client states
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sockfd = 0;
    }

    int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {0};
    int port_number = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_number);

    Bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    Listen(server_fd, 5);

    printf(" Сервер успешно запущен, порт: %d\n", port_number);
    printf(" MМаксимальное количество клиентов: %d\n", MAX_CLIENTS);
    printf(" Введите 'Close' для остановки сервера\n");

    // Core: select multiplexing main loop
    fd_set read_fds;
    int max_fd;
    int activity;

    while (1) {
        // 1. Clear and set file descriptor set
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);  // Listen to server socket
        max_fd = server_fd;

        // Add all client sockets to monitoring set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd > 0) {
                FD_SET(clients[i].sockfd, &read_fds);
                if (clients[i].sockfd > max_fd) {
                    max_fd = clients[i].sockfd;
                }
            }
        }

        // 2. Wait for socket activity (blocking call)
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select error");
            continue;
        }

        // 3. Check for new client connections
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int new_sockfd = Accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);

            // Find empty slot for new client
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].sockfd == 0) {
                    init_client_state(i, new_sockfd);
                    client_count++;
                    printf("Новый клиент подключился: сокет %d, сейчас онлайн: %d\n", new_sockfd, client_count);
                    added = 1;
                    break;
                }
            }

            if (!added) {
                write(new_sockfd, "Сервер переполнен, попробуйте позже\n", 50);
                close(new_sockfd);
            }
        }

        // 4. Check messages from all clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd > 0 && FD_ISSET(clients[i].sockfd, &read_fds)) {
                char buffer[BUFFER_SIZE];
                int n = read(clients[i].sockfd, buffer, BUFFER_SIZE - 1);

                if (n <= 0) {
                    // Client disconnected
                    printf("Client disconnected: socket %d", clients[i].sockfd);
                    if (clients[i].has_username) {
                        printf(" (%s)", clients[i].username);
                    }
                    printf("\n");

                    close(clients[i].sockfd);
                    clients[i].sockfd = 0;
                    client_count--;
                } else {
                    buffer[n] = '\0';

                    // Check if it's a server shutdown command
                    if (strstr(buffer, "Close") != NULL) {
                        printf("Выключение сервера...\n");
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].sockfd > 0) {
                                write(clients[j].sockfd, "Сервер выключается...\n", 35);
                                close(clients[j].sockfd);
                            }
                        }
                        close(server_fd);
                        printf("Сервер закрыт\n");
                        return 0;
                    }

                    // Handle client message
                    handle_client_message(i, buffer);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}