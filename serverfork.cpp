#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>  

#define PORT 9191
#define BUFFER_SIZE 1024

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("Error reading from socket");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0';
    printf("Request: %s\n", buffer);

    // Manejar métodos GET y HEAD
    if (strncmp(buffer, "GET ", 4) == 0 || strncmp(buffer, "HEAD ", 5) == 0) {
        char *file_path = strtok(buffer + (buffer[0] == 'G' ? 4 : 5), " ");
        if (file_path == NULL || strstr(file_path, "../") != NULL || strchr(file_path + 1, '/') != NULL) {
            const char *response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            write(client_socket, response, strlen(response));
        } else {
            FILE *file = fopen(file_path + 1, "r");
            if (file == NULL) {
                const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
                write(client_socket, response, strlen(response));
            } else {
                const char *response = "HTTP/1.1 200 OK\r\n\r\n";
                write(client_socket, response, strlen(response));
                if (buffer[0] == 'G') {
                    char file_buffer[BUFFER_SIZE];
                    while (fgets(file_buffer, BUFFER_SIZE, file) != NULL) {
                        write(client_socket, file_buffer, strlen(file_buffer));
                    }
                }
                fclose(file);
            }
        }
    } else {
        const char *response = "HTTP/1.1 501 Not Implemented\r\n\r\n";
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
}

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    struct sigaction sa;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        }
        close(client_socket);
    }

    close(server_socket);
    return 0;
}
