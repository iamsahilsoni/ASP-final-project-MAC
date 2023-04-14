#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 32000
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[])
{
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char *filename = "out.tar.gz";

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    // Configure server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "192.168.2.26", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Send filename to server
    send(sock, filename, strlen(filename), 0);

    // Receive file from server
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("Error opening file");
        return -1;
    }
    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0)
    {
        fwrite(buffer, sizeof(char), valread, fp);
        memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(fp);
    printf("File received successfully\n");
    return 0;
}
