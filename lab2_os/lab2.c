#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0;

void handleSigHup(int sig) { wasSigHup = 1; }

int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port)};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0 ||
        bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 || listen(server_fd, 5) < 0) {
        perror("Ошибка создание сервера");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

void configure_signal_handler() {
    struct sigaction sa = {.sa_handler = handleSigHup, .sa_flags = SA_RESTART};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("Ошибка настройки обработчика сигнала");
        exit(EXIT_FAILURE);
    }
}

int establish_client_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        perror("Ошибка принятия соединения");
        return -1;
    }
    printf("Новое соединение от %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return client_fd;
}

ssize_t receive_data(int client_fd, char *buffer) {
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read < 0) perror("Ошибка чтения данных");
        printf("Клиент пошел пить чай\n");
    } else {
        printf("Получено %zd байт\n", bytes_read);
    }
    return bytes_read;
}

int main() {
    int server_fd = create_server_socket(PORT), client_fd = -1;
    char buffer[BUFFER_SIZE];
    configure_signal_handler();

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        if (client_fd != -1) FD_SET(client_fd, &read_fds);

        if (pselect((client_fd > server_fd ? client_fd : server_fd) + 1, &read_fds, NULL, NULL, NULL, NULL) < 0) {
            if (errno == EINTR && wasSigHup) {
                printf("Получен сигнал SIGHUP\n");
                wasSigHup = 0;
                continue;
            }
            perror("Ошибка в pselect");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            if (client_fd == -1) client_fd = establish_client_connection(server_fd);
            else close(establish_client_connection(server_fd)); 
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds) && receive_data(client_fd, buffer) <= 0) {
            close(client_fd);
            client_fd = -1;
        }
    }

    close(server_fd);
    if (client_fd != -1) close(client_fd);
    return 0;
}