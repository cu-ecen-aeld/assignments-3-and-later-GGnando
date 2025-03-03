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
#include <pthread.h> 
#include "queue.h"

const char* port = "9000";

// return codes
const int success_return_code = 0;
const int failure_return_code = -1;

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
const char* file_path = "/dev/aesdchar";
#else
const char* file_path = "/var/tmp/aesdsocketdata";
#endif

// #ifdef USE_AESD_CHAR_DEVICE
// const char* file_path = "/var/tmp/aesdsocketdata";
// #else
// const char* file_path = "/dev/aesdchar";
// #endif

FILE *file = NULL;
const int backlog = 100;
volatile sig_atomic_t forever = 1;
const int buffer_size = 1024;
bool timer_created = false;
int server_fd = -1;
timer_t timerid = 0;
pthread_mutex_t lock; 

void add_timestamp_to_file()
{
    #ifndef USE_AESD_CHAR_DEVICE

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y %m %d %H %M %S %Z", tm_info);
        pthread_mutex_lock(&lock);
        fprintf(file, "timestamp:%s\n", time_str);
        pthread_mutex_unlock(&lock);
    #endif
}

typedef struct list_data_s list_data_t;

struct list_data_s {
    int client;
    FILE* file;
    pthread_mutex_t* file_mutex;
    volatile sig_atomic_t done_processing;
    pthread_t ptid;
    char client_ip[INET_ADDRSTRLEN];
    LIST_ENTRY(list_data_s) entries;
};


void clean_up_and_exit(int exit_flag);

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        forever = 0;
    }
    else if(signal == SIGALRM)
    {
        add_timestamp_to_file();
    }
}

void clean_up_and_exit(int exit_flag)
{
    if(server_fd != -1)
    {
        #ifndef USE_AESD_CHAR_DEVICE
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
        #endif
        closelog();
        close(server_fd);
    }
    if(timer_created)
    {
        timer_delete(timerid);
    }
    exit(exit_flag);
}

int become_daemon()
{
    // make process into daemon using double fork method
    // first fork
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork call failed");
        clean_up_and_exit(failure_return_code);
    }
    if (pid > 0) {
        clean_up_and_exit(success_return_code);
    }
    
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid call failed");
        clean_up_and_exit(failure_return_code);
    }
    
    // second fork
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Second fork call failed");
        clean_up_and_exit(failure_return_code);
    }
    if (pid > 0) {
        clean_up_and_exit(success_return_code);
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

void* client_thread(void* arg) 
{         
    // allocate buffer on stack
    char buffer[buffer_size*2];

    list_data_t* thread_data = (list_data_t*)arg;
    
    #if (!USE_AESD_CHAR_DEVICE)

        while(!thread_data->done_processing)
        {
            int bytes_received;
            if ((bytes_received = recv(thread_data->client, buffer, buffer_size - 1, 0)) > 0)
            {
                buffer[bytes_received] = '\0';
                pthread_mutex_lock(thread_data->file_mutex);
                fputs(buffer, thread_data->file);
                fflush(thread_data->file);
                pthread_mutex_unlock(thread_data->file_mutex); 
            }
            
            if (strchr(buffer, '\n'))
            {
                pthread_mutex_lock(thread_data->file_mutex);
                fseek(thread_data->file, 0, SEEK_SET);

                while (fgets(buffer, buffer_size, thread_data->file))
                {
                    send(thread_data->client, buffer, strlen(buffer), 0);
                }
                pthread_mutex_unlock(thread_data->file_mutex); 
        
                close(thread_data->client);
                thread_data->done_processing = 1;
            }
        }

    #else

        file = fopen(file_path, "a+");
        if (!file)
        {
            syslog(LOG_ERR, "Failed to open file");
            return NULL;
        }

        while(!thread_data->done_processing)
        {
            int bytes_received;
            if ((bytes_received = recv(thread_data->client, buffer, buffer_size - 1, 0)) > 0)
            {
                syslog(LOG_INFO, "bytes_received: %d\n", bytes_received);
                buffer[bytes_received] = '\0';
                pthread_mutex_lock(thread_data->file_mutex);
                fputs(buffer, file);
                // fwrite(buffer, 1, bytes_received, file);
                fflush(file);
                pthread_mutex_unlock(thread_data->file_mutex); 
            }
            else
            {
                syslog(LOG_INFO, "bytes_received <= 0\n");
            }
            

            if (strchr(buffer, '\n'))
            {
                syslog(LOG_INFO, "got new line\n");

                pthread_mutex_lock(thread_data->file_mutex);
                fseek(file, 0, SEEK_SET);
                ssize_t read_bytes = fread(buffer, 1, buffer_size -1, file);
                syslog(LOG_ERR, "read_bytes %ld\n", read_bytes);
                while(read_bytes > 0)
                // while (fgets(buffer, buffer_size, file))
                {
                    ssize_t bytes_sent = send(thread_data->client, buffer, strlen(buffer), 0);
                    syslog(LOG_INFO, "bytes_sent %ld\n", bytes_sent);
                    read_bytes= fread(buffer, 1, buffer_size -1, file);
                    syslog(LOG_INFO, "read_bytes %ld\n", read_bytes);
                }
                pthread_mutex_unlock(thread_data->file_mutex); 
        
                close(thread_data->client);
                thread_data->done_processing = 1;
            } 
        }
        fclose(file);

    #endif

    syslog(LOG_INFO, "Closed connection from %s", thread_data->client_ip);

    close(thread_data->client);
    return NULL;
}

int main(int argc, char *argv[])
{
    int daemon_requested = false;
    
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_requested = true;
    }
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

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

    if (daemon_requested) {
        become_daemon();
    }


    struct sigevent sev;
    struct itimerspec timer_spec;

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &timerid;

    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        syslog(LOG_ERR, "Error creating timer");
        return failure_return_code;
    }
    else
    {
        timer_created = true;
    }
    

    LIST_HEAD(listhead, list_data_s) head;
    LIST_INIT(&head);

    timer_spec.it_value.tv_sec = 10;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 10;
    timer_spec.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &timer_spec, NULL) == -1) {
        syslog(LOG_ERR, "Error setting interval timer %d", errno);
        return failure_return_code; 
    }

    // Set up signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    
    if (listen(server_fd, backlog) == -1) 
    {
        syslog(LOG_ERR, "Listen call failed");
        close(server_fd);
        return failure_return_code;
    }

    if (pthread_mutex_init(&lock, NULL) != 0) { 
        syslog(LOG_ERR, "Mutex Init failed");
        return failure_return_code; 
    } 

    #if (!USE_AESD_CHAR_DEVICE)

        file = fopen(file_path, "a+");
        if (!file)
        {
            syslog(LOG_ERR, "Failed to open file");
            close(server_fd);
        }
    #endif

    while(forever)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &address_size);
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

            list_data_t* thread_data = malloc(sizeof(list_data_t));
            if(thread_data)
            {
                thread_data->client          = client_fd;
                thread_data->file            = file;
                thread_data->file_mutex      = &lock;
                thread_data->done_processing = 0;
                memcpy(thread_data->client_ip, client_ip, INET_ADDRSTRLEN);

                if ((pthread_create(&thread_data->ptid, NULL, client_thread, thread_data)) != 0)
                {
                    syslog(LOG_ERR, "Failed to spawn thread");
                    free(thread_data);
                    close(client_fd);
                    continue;
                }
                LIST_INSERT_HEAD(&head, thread_data, entries);
            }
            else
            {
                syslog(LOG_ERR, "Failed to allocate thread storage");
                close(client_fd);
                continue;
            }
        }
    }

    // join threads
    list_data_t *datap=NULL;
    while (!LIST_EMPTY(&head)) {
        datap = LIST_FIRST(&head);
        pthread_join(datap->ptid, NULL); 
        LIST_REMOVE(datap, entries);
        free(datap);
    }

    clean_up_and_exit(success_return_code);
    return success_return_code;
}