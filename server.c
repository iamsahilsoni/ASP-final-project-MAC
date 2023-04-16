// Sahil Soni(110093229), Aashi Thakkar(110093562)
// University of Windsor
// Advanced System Programming - Server Client Project
// April 15th 2023

// Server-Side code
// Usage :-
// 1. gcc -o server server.c
// 2. ./server

// Server & Mirror are almost same,
// except Server is rejecting the client based on Load (conditions mentioned in Project description).

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/ip.h>
#include <fnmatch.h>

#define PORT 8080
#define MAX_RESPONSE_SIZE 1024
#define MAX_ARGUMENTS 10
#define CHUNK_SIZE 16384
#define MAX_BUFFER_SIZE 1024
#define MAX_EXTENSION_COUNT 6
#define RESPONSE_TEXT 1
#define RESPONSE_FILE 2
#define RESPONSE_QUIT 3

int client_count = 0;

/*-------------------------COMMON----------------------------------*/

// This function takes a file name as input and returns the size of the file in bytes
long int getFileSize(const char *filename)
{
    FILE *fp = fopen(filename, "rb"); // open the file in binary mode
    // Check if the file was opened successfully
    if (fp == NULL)
    {
        perror("Error opening file");
        return -1; // Return -1 to indicate an error
    }

    fseek(fp, 0, SEEK_END);    // Move the file pointer to the end of the file
    long int size = ftell(fp); // Get the current position of the file pointer, which is the file size in bytes
    fclose(fp);                // Close the file

    return size; // Return the file size
}

/*----------------------------FINDFILE----------------------------*/

/**
 *This function searches for a file in the home directory and returns the file information if found.
 *@param client_sockfd the socket file descriptor for the client connection
 *@param arguments an array of arguments for the function call, where arguments[1] is the filename to search for
 *@return void
 */
void findfile(int client_sockfd, char **arguments)
{
    char *filename = arguments[1];
    char response[1024];

    char *home_dir = getenv("HOME");                                          // Get the home directory path
    char *command = (char *)malloc(strlen(home_dir) + strlen(filename) + 27); // Allocate memory for the command string
    sprintf(command, "find %s -name '%s' -print -quit", home_dir, filename);  // Construct the find command
    FILE *pipe = popen(command, "r");                                         // Open a pipe to the command
    if (pipe != NULL)
    {
        char line[256];
        if (fgets(line, sizeof(line), pipe) != NULL)
        {                                     // Read the first line of output
            line[strcspn(line, "\n")] = '\0'; // Remove the newline character from the end of the line
            struct stat sb;
            if (stat(line, &sb) == 0)
            {                                                                                                                                      // Get the file information using stat()
                sprintf(response, "Name of file is: %s,\nSize is: %lld bytes,\nCreated on: %s", line, (long long)sb.st_size, ctime(&sb.st_ctime)); // Print the file information
            }
            else
            {
                sprintf(response, "Unable to get file information for %s\n", line);
            }
        }
        else
        {
            sprintf(response, "File not found\n");
        }
        pclose(pipe); // Close the pipe
    }
    else
    {
        printf("Error opening pipe to command\n");
    }
    free(command); // Free the memory allocated for the command string

    // Send text response
    int response_type = RESPONSE_TEXT;
    write(client_sockfd, &response_type, sizeof(response_type));

    write(client_sockfd, response, strlen(response));
}

/*-----------------------------SGETFILES----------------------------*/

bool check_size(char *path, size_t size1, size_t size2)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= size1 && st.st_size <= size2)
    {
        return true;
    }
    return false;
}

/**
 * This function takes a directory path, two size limits, and a file descriptor as input.
 * It recursively traverses the directory and its subdirectories, printing the path of each file whose size is within the given limits.
 * The path of each file is written to the file descriptor in the format "path\n".
 *
 * @param dirpath The directory path to traverse.
 * @param size1 The lower size limit (in bytes) of files to include.
 * @param size2 The upper size limit (in bytes) of files to include.
 * @param tmp_fd The file descriptor to write the file paths to.
 * @return void.
 */
void traverse_directory(char *dirpath, size_t size1, size_t size2, int tmp_fd)
{
    DIR *dir = opendir(dirpath); // open the directory specified by `dirpath`
    if (dir == NULL)             // check if the directory is opened successfully
    {
        perror("Error opening directory");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) // read each directory entry in the directory
    {
        char path[MAX_RESPONSE_SIZE];                 // buffer to store the path of the file or directory
        sprintf(path, "%s/%s", dirpath, ent->d_name); // construct the path of the file or directory by concatenating `dirpath` and `ent->d_name`
        if (check_size(path, size1, size2))           // check if the file size is between `size1` and `size2` (inclusive)
        {
            write(tmp_fd, path, strlen(path)); // write the path of the file to the file descriptor `tmp_fd`
            write(tmp_fd, "\n", 1);            // write a newline character to the file descriptor `tmp_fd`
        }
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) // check if the directory entry is a directory and not `.` or `..`
        {
            traverse_directory(path, size1, size2, tmp_fd); // recursively traverse the subdirectory
        }
    }
    closedir(dir); // close the directory
}

/**
 * sgetfiles - creates a tar file of all files in user's home directory
 *             with a size greater than or equal to size1 and less than
 *             or equal to size2, and sends the tar file to the client
 *
 * @param client_sockfd: file descriptor of the client socket
 * @param arguments: an array of arguments containing the size1 and size2 values
 */
void sgetfiles(int client_sockfd, char **arguments)
{
    // Get the user's home directory and the name of the temporary tar file
    char *dirname = getenv("HOME");
    char *filename = "temp.tar.gz";

    // Convert the size arguments to integers
    size_t size1 = atoi(arguments[1]);
    size_t size2 = atoi(arguments[2]);

    // Initialize a buffer to store file data
    char buf[MAX_RESPONSE_SIZE];

    // Initialize a pipe and a process ID
    int fd[2], nbytes;
    pid_t pid;

    // Create a temporary file to store the list of files
    char tmp_file[] = "tempXXXXXX";
    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd < 0)
    {
        perror("Error creating temporary file");
        return;
    }

    // Traverse the directory tree and write the filenames to the temporary file
    traverse_directory(dirname, size1, size2, tmp_fd);

    // Fork a child process to handle the tar and zip operation
    if (pipe(fd) < 0)
    {
        perror("Error creating pipe");
        return;
    }
    pid = fork();
    if (pid < 0)
    {
        perror("Error forking child process");
        return;
    }
    else if (pid == 0)
    {
        // Child process - execute tar command
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execlp("tar", "tar", "-czf", filename, "-T", tmp_file, NULL);
        perror("Error executing tar command");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process - send the tar file to the client
        close(fd[1]);

        // Send the file response header
        int response_type = 2;
        write(client_sockfd, &response_type, sizeof(response_type));

        // Wait for the child process to complete
        wait(NULL);
        fflush(stdout);

        // Get the size of the tar file
        long int filesize = getFileSize("temp.tar.gz");
        write(client_sockfd, &filesize, sizeof(filesize));

        // Delete the temporary file
        unlink(tmp_file);

        // Send the tar file to the client
        int tar_fd = open(filename, O_RDONLY);
        if (tar_fd < 0)
        {
            perror("Error opening tar file");
            return;
        }
        ssize_t bytes_read, bytes_sent;
        off_t bytes_remaining = filesize;
        while (bytes_remaining > 0 && (bytes_read = read(tar_fd, buf, sizeof(buf))) > 0)
        {
            bytes_sent = send(client_sockfd, buf, bytes_read, 0);
            if (bytes_sent < 0)
            {
                perror("Error sending file data");
                break;
            }
            bytes_remaining -= bytes_sent;
        }
        close(tar_fd);

        // Delete the tar file
        unlink(filename);
    }
}

/*---------------------------DGETFILES----------------------------*/

bool check_date(char *path, time_t time1, time_t time2)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_mtime >= time1 && st.st_mtime <= time2)
    {
        return true;
    }
    return false;
}

void traverse_directory_for_date(char *dirpath, time_t time1, time_t time2, int tmp_fd)
{
    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        perror("Error opening directory");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        char path[MAX_RESPONSE_SIZE];
        sprintf(path, "%s/%s", dirpath, ent->d_name);
        if (check_date(path, time1, time2))
        {
            write(tmp_fd, path, strlen(path));
            write(tmp_fd, "\n", 1);
        }
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
        {
            traverse_directory_for_date(path, time1, time2, tmp_fd);
        }
    }
    closedir(dir);
}

// The functions sgetfiles and dgetfiles have a high degree of similarity to each other.
// While sgetfiles selects files based on their sizes, dgetfiles selects files based on their dates.

void dgetfiles(int client_sockfd, char **arguments)
{
    char *dirname = getenv("HOME");
    char *filename = "temp.tar.gz";
    char *date1_str = arguments[1];
    char *date2_str = arguments[2];

    struct tm date1_tm, date2_tm;
    sscanf(date1_str, "%d-%d-%d", &date1_tm.tm_year, &date1_tm.tm_mon, &date1_tm.tm_mday);
    sscanf(date2_str, "%d-%d-%d", &date2_tm.tm_year, &date2_tm.tm_mon, &date2_tm.tm_mday);
    date1_tm.tm_year -= 1900;
    date1_tm.tm_mon -= 1;
    date1_tm.tm_hour = 0;
    date1_tm.tm_min = 0;
    date1_tm.tm_sec = 0;
    date2_tm.tm_year -= 1900;
    date2_tm.tm_mon -= 1;
    date2_tm.tm_hour = 23;
    date2_tm.tm_min = 59;
    date2_tm.tm_sec = 59;
    time_t date1 = mktime(&date1_tm);
    time_t date2 = mktime(&date2_tm);

    char buf[MAX_RESPONSE_SIZE];
    int fd[2], nbytes;
    pid_t pid;

    // Create a temporary file to store the list of files
    char tmp_file[] = "tempXXXXXX";
    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd < 0)
    {
        perror("Error creating temporary file");
        return;
    }

    // Traverse the directory tree and write the filenames
    traverse_directory_for_date(dirname, date1, date2, tmp_fd);

    // Fork a child process to handle the tar and zip operation
    if (pipe(fd) < 0)
    {
        perror("Error creating pipe");
        return;
    }
    pid = fork();
    if (pid < 0)
    {
        perror("Error forking child process");
        return;
    }
    else if (pid == 0)
    {
        // Child process
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execlp("tar", "tar", "-czf", filename, "-T", tmp_file, NULL);
        perror("Error executing tar command");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        close(fd[1]);
        // Send file response
        int response_type = 2;
        write(client_sockfd, &response_type, sizeof(response_type));

        wait(NULL);
        fflush(stdout);
        long int filesize = getFileSize("temp.tar.gz");
        write(client_sockfd, &filesize, sizeof(filesize));

        // Delete the temporary file
        unlink(tmp_file);

        // Send the tar file to the client
        int tar_fd = open(filename, O_RDONLY);
        if (tar_fd < 0)
        {
            perror("Error opening tar file");
            return;
        }
        ssize_t bytes_read, bytes_sent;
        off_t bytes_remaining = filesize;
        while (bytes_remaining > 0 && (bytes_read = read(tar_fd, buf, sizeof(buf))) > 0)
        {
            bytes_sent = send(client_sockfd, buf, bytes_read, 0);
            if (bytes_sent < 0)
            {
                perror("Error sending file data");
                break;
            }
            bytes_remaining -= bytes_sent;
        }
        close(tar_fd);

        // Delete the tar file
        unlink(filename);
    }
}

/*-------------------------GETFILES-------------------------*/

/**
 * Traverses a directory tree and writes the paths of files that match any of the specified filenames to a temporary file.
 *
 * @param dirpath The path of the directory to traverse.
 * @param filenames An array of strings containing the filenames to match.
 * @param num_files The number of filenames in the array.
 * @param tmp_fd The file descriptor for the temporary file.
 */
void traverse_directory_for_getfiles(char *dirpath, char **filenames, int num_files, int tmp_fd)
{
    // Open the directory with the given path
    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        perror("Error opening directory");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        // Create the full path of the current file
        char path[MAX_RESPONSE_SIZE];
        sprintf(path, "%s/%s", dirpath, ent->d_name);

        // Check if the current file matches any of the specified filenames
        for (int i = 0; i < num_files; i++)
        {
            if (strcmp(ent->d_name, filenames[i]) == 0)
            {
                // Write the path of the matching file to the temporary file descriptor
                write(tmp_fd, path, strlen(path));
                write(tmp_fd, "\n", 1);
                break;
            }
        }

        // Recursively traverse any subdirectories
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
        {
            traverse_directory_for_getfiles(path, filenames, num_files, tmp_fd);
        }
    }
    // Close the directory
    closedir(dir);
}

/**
 * @brief Retrieves files from the user's home directory that match the specified filenames,
 *        creates a tar.gz archive of those files and sends it to the client.
 *
 * @param client_sockfd The socket file descriptor for the connected client
 * @param arguments An array of strings containing the filenames to search for
 * @param argLen The length of the arguments array
 */
void getfiles(int client_sockfd, char **arguments, int argLen)
{
    char *dirname = getenv("HOME");
    char *filename = "temp.tar.gz";

    char buf[MAX_RESPONSE_SIZE];
    int fd[2], nbytes;
    pid_t pid;

    // Create a temporary file to store the list of files
    char tmp_file[] = "tempXXXXXX";
    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd < 0)
    {
        perror("Error creating temporary file");
        return;
    }

    // Traverse the specified directory and its subdirectories to find the requested files
    traverse_directory_for_getfiles(dirname, arguments + 1, argLen - 1, tmp_fd);

    // Check if any files were found
    if (lseek(tmp_fd, 0, SEEK_END) == 0)
    {
        // No files found, send "No file found" response
        int response_type = 1;
        write(client_sockfd, &response_type, sizeof(response_type));
        write(client_sockfd, "No file found", 13);
        close(tmp_fd);
        unlink(tmp_file);
        return;
    }

    // Fork a child process to handle the tar and zip operation
    if (pipe(fd) < 0)
    {
        perror("Error creating pipe");
        return;
    }
    pid = fork();
    if (pid < 0)
    {
        perror("Error forking child process");
        return;
    }
    else if (pid == 0)
    {
        // Child process
        close(fd[0]);
        // Redirect standard output to the write end of the pipe
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        // Execute the tar command to compress the files into a single archive
        execlp("tar", "tar", "-czf", filename, "-T", tmp_file, NULL);
        perror("Error executing tar command");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        close(fd[1]);
        // Send file response
        int response_type = 2;
        write(client_sockfd, &response_type, sizeof(response_type));

        wait(NULL);
        fflush(stdout);
        // Get the size of the tar file
        long int filesize = getFileSize("temp.tar.gz");
        write(client_sockfd, &filesize, sizeof(filesize));

        // Delete the temporary file
        unlink(tmp_file);

        // Send the tar file to the client
        int tar_fd = open(filename, O_RDONLY);
        if (tar_fd < 0)
        {
            perror("Error opening tar file");
            return;
        }
        ssize_t bytes_read, bytes_sent;
        off_t bytes_remaining = filesize;
        // Send the tar file to the client in chunks
        while (bytes_remaining > 0 && (bytes_read = read(tar_fd, buf, sizeof(buf))) > 0)
        {
            bytes_sent = send(client_sockfd, buf, bytes_read, 0);
            if (bytes_sent < 0)
            {
                perror("Error sending file data");
                break;
            }
            bytes_remaining -= bytes_sent;
        }
        close(tar_fd);

        // Delete the tar file
        unlink(filename);
    }
}

/*--------------------------GETTARGZ---------------------------*/

void traverse_directory_for_gettargz(char *dirpath, char **extensions, int num_extensions, int tmp_fd)
{
    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        perror("Error opening directory");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        char path[MAX_RESPONSE_SIZE];
        sprintf(path, "%s/%s", dirpath, ent->d_name);

        // Check if the current file matches any of the specified filenames
        char *ext = strrchr(ent->d_name, '.');
        if (ext != NULL && ext != ent->d_name && strlen(ext) <= 5)
        {
            ext++; // skip the dot character
            for (int i = 0; i < num_extensions; i++)
            {
                if (strcmp(ext, extensions[i]) == 0)
                {
                    write(tmp_fd, path, strlen(path));
                    write(tmp_fd, "\n", 1);
                    break;
                }
            }
        }

        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
        {
            traverse_directory_for_gettargz(path, extensions, num_extensions, tmp_fd);
        }
    }
    closedir(dir);
}

// The functions getfiles and gettargz have a high degree of similarity to each other.
// While getfiles selects files based on entered names, dgetfiles selects files based on entered extensions.

void gettargz(int client_sockfd, char **arguments, int argLen)
{
    char *dirname = getenv("HOME");
    char *filename = "temp.tar.gz";

    char buf[MAX_RESPONSE_SIZE];
    int fd[2], nbytes;
    pid_t pid;

    // Create a temporary file to store the list of files
    char tmp_file[] = "tempXXXXXX";
    int tmp_fd = mkstemp(tmp_file);
    if (tmp_fd < 0)
    {
        perror("Error creating temporary file");
        return;
    }
    traverse_directory_for_gettargz(dirname, arguments + 1, argLen - 1, tmp_fd);

    // Check if any files were found
    if (lseek(tmp_fd, 0, SEEK_END) == 0)
    {
        // No files found, send "No file found" response
        int response_type = 1;
        write(client_sockfd, &response_type, sizeof(response_type));
        write(client_sockfd, "No file found", 13);
        close(tmp_fd);
        unlink(tmp_file);
        return;
    }

    // Fork a child process to handle the tar and zip operation
    if (pipe(fd) < 0)
    {
        perror("Error creating pipe");
        return;
    }
    pid = fork();
    if (pid < 0)
    {
        perror("Error forking child process");
        return;
    }
    else if (pid == 0)
    {
        // Child process
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        execlp("tar", "tar", "-czf", filename, "-T", tmp_file, NULL);
        perror("Error executing tar command");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        close(fd[1]);
        // Send file response
        int response_type = 2;
        write(client_sockfd, &response_type, sizeof(response_type));

        wait(NULL);
        fflush(stdout);
        long int filesize = getFileSize("temp.tar.gz");
        write(client_sockfd, &filesize, sizeof(filesize));

        // Delete the temporary file
        unlink(tmp_file);

        // Send the tar file to the client
        int tar_fd = open(filename, O_RDONLY);
        if (tar_fd < 0)
        {
            perror("Error opening tar file");
            return;
        }

        ssize_t bytes_read, bytes_sent;
        off_t bytes_remaining = filesize;
        while (bytes_remaining > 0 && (bytes_read = read(tar_fd, buf, sizeof(buf))) > 0)
        {
            bytes_sent = send(client_sockfd, buf, bytes_read, 0);
            if (bytes_sent < 0)
            {
                perror("Error sending file data");
                break;
            }
            bytes_remaining -= bytes_sent;
        }
        close(tar_fd);

        // Delete the tar file
        unlink(filename);
    }
}

/*---------------------PROCESSCLIENT-----------------------*/

/**
 * Removes line breaks from each token in the given array of strings.
 *
 * @param tokens      array of strings
 * @param num_tokens  number of strings in the array
 */
void remove_linebreak(char **tokens, int num_tokens)
{
    for (int i = 0; i < num_tokens; i++)
    {
        char *token = tokens[i];
        int length = strcspn(token, "\n");            // get length of token up to the line break
        char *new_token = (char *)malloc(length + 1); // allocate memory for new token
        strncpy(new_token, token, length);            // copy characters up to line break from original token to new token
        new_token[length] = '\0';                     // add null terminator to new token
        tokens[i] = new_token;                        // replace original token with new token
    }
}

/**
 *   The function receives a socket descriptor and reads the command sent by the client
 *   It then tokenizes the command into arguments, removes line breaks from the arguments
 *   and processes the command by calling corresponding functions. If the command is invalid,
 *   it generates an error response and waits for a new command. The function runs indefinitely
 *   until the client disconnects or sends a "quit" command.
 *   @param client_sockfd A socket descriptor for the client connection
 *   @return void
 */
void processClient(int client_sockfd)
{
    char command[255];
    char response[1024];
    int n, pid;
    write(client_sockfd, "Send commands", 14);

    while (1)
    {
        memset(command, 0, sizeof(command));
        n = read(client_sockfd, command, 255);
        // Check if client has disconnected
        if (n <= 0)
        {
            printf("Client disconnected.\n");
            break; // Exit loop and terminate processclient() function
        }

        char *arguments[MAX_ARGUMENTS]; // Array to store command arguments
        int num_arguments = 0;          // Counter for number of arguments

        // Tokenize the command into arguments
        char *token = strtok(command, " "); // Tokenize command using space as delimiter

        while (token != NULL)
        {
            arguments[num_arguments++] = token; // Store the token in the array
            token = strtok(NULL, " ");          // Get the next token
        }
        arguments[num_arguments] = NULL; // Set the last element of the array to NULL

        // Remove line breaks from tokens
        remove_linebreak(arguments, num_arguments);

        char *cmd = arguments[0]; // Extract first token as the command

        printf("\nExecuting the following command: \n");
        for (int i = 0; i < num_arguments; i++)
        {
            printf("%s ", arguments[i]);
        }
        printf("\n\n");

        // Process the command and generate response
        if (strcmp(cmd, "findfile") == 0)
        {
            findfile(client_sockfd, arguments);
        }
        else if (strcmp(cmd, "sgetfiles") == 0)
        {
            sgetfiles(client_sockfd, arguments);
        }
        else if (strcmp(cmd, "dgetfiles") == 0)
        {
            dgetfiles(client_sockfd, arguments);
        }
        else if (strcmp(cmd, "getfiles") == 0)
        {
            getfiles(client_sockfd, arguments, num_arguments);
        }
        else if (strcmp(cmd, "gettargz") == 0)
        {
            gettargz(client_sockfd, arguments, num_arguments);
        }
        else if (strcmp(cmd, "quit") == 0)
        {
            printf("Client requested to quit.\n");
            int response_type = RESPONSE_QUIT;
            write(client_sockfd, &response_type, sizeof(response_type));
            break; // Exit loop and terminate processclient() function
        }
        else
        {
            snprintf(response, MAX_RESPONSE_SIZE, "Invalid command\n"); // Generate response for invalid command
            write(client_sockfd, response, strlen(response));
            continue; // Continue to next iteration of loop to wait for new command
        }
    }
    exit(0);
}

/*-------------MAIN----------------------*/

// Standard main function with arguments count and argument vector

int main(int argc, char *argv[])
{
    int sd, csd, portNumber, status;
    socklen_t len;
    struct sockaddr_in servAdd; // ipv4

    // Create a socket with domain: Internet, type: Stream and protocol: TCP
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Cannot create socket\n");
        exit(1);
    }

    // Add information to the servAdd struct variable of type sockaddr_in
    servAdd.sin_family = AF_INET;
    // When INADDR_ANY is specified in the bind call, the socket will be bound to all local interfaces.
    // The htonl function takes a 32-bit number in host byte order and returns a 32-bit number in the network byte order used in TCP/IP networks
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY); // Host to network long

    // htons: Host to network short-byte order
    servAdd.sin_port = htons(PORT);

    // Bind the socket to the address and port
    bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd));

    // Listen for incoming connections on the bound socket with maximum number of clients allowed to connect
    listen(sd, 5);

    printf("Server started, waiting for client... !\n");

    // Wait for clients to connect
    while (1)
    {
        // Accept a client connection, and get a new socket descriptor (csd) to communicate with the client
        csd = accept(sd, (struct sockaddr *)NULL, NULL);
        client_count++;
        printf("Client Count: %d\n", client_count);

        int willAccept = 0;
        if (client_count <= 4)
        {
            willAccept = 1;
        }
        else if (client_count >= 5 && client_count <= 8)
        {
            willAccept = 0;
        }
        else
        {
            if ((client_count - 9) % 2 == 0)
            {
                willAccept = 1;
            }
            else
            {
                willAccept = 0;
            }
        }

        // If there is space for the client to connect
        if (willAccept)
        {
            printf("Got a client\n");

            // Send a message to the client to indicate that it can connect
            int response_type = 1;
            write(csd, &response_type, sizeof(response_type));

            // Create a new process to handle the client communication
            if (!fork()) // Child process
                processClient(csd);

            // Close the socket descriptor
            close(csd);

            // Wait for any child processes to finish
            waitpid(0, &status, WNOHANG);
        }
        // If there is no space for the client to connect
        else
        {
            // Send a message to the client to indicate that it cannot connect
            int response_type = 0;
            write(csd, &response_type, sizeof(response_type));

            // Close the socket descriptor
            close(csd);
        }
    }
}
