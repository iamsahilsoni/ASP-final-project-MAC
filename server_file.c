#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 32000
#define FILENAME "temp.tar.gz"

int main(int argc, char const *argv[])
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address and port
    /*int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }*/

    // Set address family, IP address, and port number
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    // Open file to be transferred
    int fd = open(FILENAME, O_RDONLY);
    if (fd < 0)
    {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    // Read file contents and send to client
    while ((valread = read(fd, buffer, 1024)) > 0)
    {
        send(new_socket, buffer, valread, 0);
    }

    // Close file and socket
    close(fd);
    close(new_socket);
    close(server_fd);

    return 0;
}
