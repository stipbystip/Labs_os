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

void sigHupHandler(int sig) { wasSigHup = 1; }

int set_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port)};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0 ||
        bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 || listen(server_fd, 5) < 0) {
        perror("о нет все пропало");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

void setup_signal_handler() {
    struct sigaction sa = {.sa_handler = sigHupHandler, .sa_flags = SA_RESTART};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int accept_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }
    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return client_fd;
}

ssize_t read_data_client(int client_fd, char *buffer) {
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read < 0) perror("recv");
        printf("Client disconnected\n");
    } else {
        printf("Received %zd bytes\n", bytes_read);
    }
    return bytes_read;
}

int main() {
    int server_fd = set_server_socket(PORT), client_fd = -1;
    char buffer[BUFFER_SIZE];
    setup_signal_handler();

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        if (client_fd != -1) FD_SET(client_fd, &read_fds);

        if (pselect((client_fd > server_fd ? client_fd : server_fd) + 1, &read_fds, NULL, NULL, NULL, NULL) < 0) {
            if (errno == EINTR && wasSigHup) {
                printf("SIGHUP received\n");
                wasSigHup = 0;
                continue;
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            if (client_fd == -1) client_fd = accept_connection(server_fd);
            else close(accept_connection(server_fd)); // Reject additional connections
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds) && read_data_client(client_fd, buffer) <= 0) {
            close(client_fd);
            client_fd = -1;
        }
    }

    close(server_fd);
    if (client_fd != -1) close(client_fd);
    return 0;
}
