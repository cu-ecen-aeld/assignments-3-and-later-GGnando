#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>

const char* port = "9000";

// return codes
const int success_return_code = 0;
const int failure_return_code = -1;
const char* file_path = "/var/tmp/aesdsocketdata";
FILE *file = NULL;
const int backlog = 100;
volatile sig_atomic_t forever = 1;
const int buffer_size = 1024;

int server_fd = -1;
int client_fd = -1;

void clean_up_and_exit(int exit_flag);

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        forever = 0;
    }
}

void clean_up_and_exit(int exit_flag)
{
    if(server_fd != -1)
    {
        if (file != NULL)
        {
            fclose(file);
            file = NULL;
            int ret_remove = remove(file_path);
            if(ret_remove != 0)
            {
                syslog(LOG_ERR, "error removing file %s", file_path);
            }
        }

        closelog();
        close(server_fd);
        if(client_fd)
        {
            close(server_fd);
        }
    }
    exit(exit_flag);
}

int become_daemon()
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork call failed");
        clean_up_and_exit(failure_return_code);
    }
    if (pid > 0) {
        exit(0);
    }
    
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid call failed");
        clean_up_and_exit(failure_return_code);
    }
    
    
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Second fork call failed");
        clean_up_and_exit(failure_return_code);
    }
    if (pid > 0) {
        exit(0);
    }
    
    umask(0);
    if (chdir("/") == -1) {
        syslog(LOG_ERR, "Failed to change directory to /");
        clean_up_and_exit(failure_return_code);
    }

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        syslog(LOG_ERR, "Failed to open /dev/null");
        clean_up_and_exit(failure_return_code);
    }
    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);
    return success_return_code;
}


int main(int argc, char *argv[])
{
    int daemon_requested = false;
    
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_requested = true;
    }
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // Set up signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    //allocate buffer on stack
    char buffer[buffer_size];

    // setup socket for server
    struct addrinfo hints, *res;
    struct sockaddr_in client_address;
    socklen_t address_size = sizeof(client_address);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, port, &hints, &res) != 0) 
    {
        syslog(LOG_ERR, "getaddrinfo call failed");
        return failure_return_code;
    }

    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd == -1) 
    {
        syslog(LOG_ERR, "Socket call failed");
        freeaddrinfo(res);
        return failure_return_code;
    }

    int option = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) {
        syslog(LOG_ERR, "setsockopt call failed");
        close(server_fd);
        freeaddrinfo(res);
        return failure_return_code;
    }

    if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1) 
    {
        syslog(LOG_ERR, "Bind call failed");
        close(server_fd);
        freeaddrinfo(res);
        return failure_return_code;
    }
    
    freeaddrinfo(res);
    
    if (listen(server_fd, backlog) == -1) 
    {
        syslog(LOG_ERR, "Listen call failed");
        close(server_fd);
        return failure_return_code;
    }

    if (daemon_requested) {
        become_daemon();
    }

    file = fopen(file_path, "a+");
    if (!file)
    {
        syslog(LOG_ERR, "Failed to open file");
        close(server_fd);
    }

    while(forever)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &address_size);
        if (client_fd == -1)
        {
            syslog(LOG_ERR, "Accept call failed");
            continue;
        }
        else
        {
            // get client ip and port info
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));
            syslog(LOG_INFO, "Accepted connection from %s", client_ip);

            int bytes_received;

            while ((bytes_received = recv(client_fd, buffer, buffer_size - 1, 0)) > 0)
            {
                buffer[bytes_received] = '\0';
                fputs(buffer, file);
                fflush(file);
                if (strchr(buffer, '\n'))
                {
                    break;
                } 
            }

            fseek(file, 0, SEEK_SET);

            while (fgets(buffer, buffer_size, file))
            {
                send(client_fd, buffer, strlen(buffer), 0);
            }

            syslog(LOG_INFO, "Closed connection from %s", client_ip);

            close(client_fd);
        }

        
    }

    clean_up_and_exit(success_return_code);

    return success_return_code;
}