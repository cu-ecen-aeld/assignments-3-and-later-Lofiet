#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#define PORT "9000"
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

volatile sig_atomic_t operate = 1;

void termination_handler(int signum)
{
    operate = 0;
}

typedef struct buff_ll{
    char buff[BUFFER_SIZE];
    struct buff_ll *next;
} buff_ll;

void free_buff_ll(buff_ll *node)
{
    buff_ll *cur_buff = node;
    buff_ll *prv_buff = NULL;
    while (cur_buff != NULL) {
       prv_buff = cur_buff;
       cur_buff = cur_buff->next;
       free(prv_buff); 
    }
    node = NULL;
}

buff_ll* create_buff_ll(void)
{
    buff_ll* newNode = (buff_ll*)malloc(sizeof(buff_ll));
    if (newNode == NULL) {
        syslog(LOG_ERR, "malloc failed");
        return NULL;
    }
    memset(newNode->buff, 0, BUFFER_SIZE);
    newNode->next = NULL;
    return newNode;
}

buff_ll* get_last(buff_ll* node)
{
    if (node == NULL) {
        return NULL;
    }
    else if (node->next != NULL) {
        return get_last(node->next);
    }
    else {
        return node;
    }
}   

buff_ll* remove_front(buff_ll* node)
{
    buff_ll* rm_node = node;
    if (node != NULL) {
        node = node->next;
    }
    return rm_node;
}

buff_ll* add_buff_ll(buff_ll *node)
{
    buff_ll* add_node = create_buff_ll();
    buff_ll* last_node = get_last(node);

    if (node == NULL) {
        node = add_node;
        if (node == NULL) {
            syslog(LOG_ERR, "Failed to set node, when node was NULL");
            return NULL;
        }
    }
    else {
        if (last_node == NULL) {
            syslog(LOG_ERR, "Failed to get last node");
            return NULL;
        }
        last_node->next = add_node;
        if (last_node->next == NULL) {
            syslog(LOG_ERR, "last_node->next is NULL");
            return NULL;
        }
    }
    return add_node;
}

void write_out(int file_fd, int socket_fd, buff_ll* output, ssize_t last_buff_size)
{
    buff_ll* cur_node = output;
    ssize_t out_size = 0;
    ssize_t write_size = 0;
    int count = 0;
    char buff[BUFFER_SIZE] = {0};

    lseek(file_fd, 0, SEEK_END);

    while(cur_node != NULL) {
        syslog(LOG_ERR, "writing buff %d", count++);
        syslog(LOG_ERR, "%s", cur_node->buff);
        if (cur_node->next == NULL) {
            out_size = last_buff_size;
        }
        else {
            out_size = strlen(cur_node->buff);
        }
        // write file
        write_size = write(file_fd, cur_node->buff, out_size);

        // write to socket
        cur_node = cur_node->next;
    }

    lseek(file_fd, 0, SEEK_SET);

    write_size = 1;
    while (write_size > 0) {
        memset(buff, 0, BUFFER_SIZE);
        write_size = read(file_fd, buff, BUFFER_SIZE);
        if (write_size > 0) {
            send(socket_fd, buff, write_size, 0);
        }
    }
}

int main (int argc, char *argv[])
{
    pid_t pid = 0;
    // pid_t wait_pid = 0;
    // int wstatus = 0;
    int socket_fd = 0;
    int accepted_socket_fd = 0;
    int file_fd = 0;
    int status = 0;
    int yes = 1;
    buff_ll* node_base = NULL;
    buff_ll* cur_node = NULL;
    buff_ll* next_node = NULL;
    char* found_newline = NULL;
    // char buff[BUFFER_SIZE] = {0};
    char ip_addr[INET_ADDRSTRLEN] = {0};
    ssize_t bytes = 0;
    struct addrinfo hints = {0};
    struct addrinfo *servinfo = NULL;
    struct sigaction action = {0};
    // bool return_val = false;
    
    openlog(argv[0], LOG_PID | LOG_ODELAY | LOG_PERROR, LOG_USER);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(NULL, PORT, &hints, &servinfo);

    if(status != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: \n%s", gai_strerror(status));
        close(socket_fd);
        freeaddrinfo(servinfo);
        return -1;
    }

    if (servinfo == NULL) {
        syslog(LOG_ERR, "servinfo is a nullptr");
        close(socket_fd);
        return -1;
    }

    if (servinfo->ai_addr == NULL) {
        syslog(LOG_ERR, "servinfo->ai_addr is a nullptr");
        close(socket_fd);
        freeaddrinfo(servinfo);
        return -1;
    }

    // get socket
    socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (socket_fd == 0) {
        syslog(LOG_ERR, "Socket fd(%d) was not created correctly", socket_fd);
        return -1;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        syslog(LOG_ERR, "setsockopt\n%s", strerror(errno));
        return -1;
    }

    status = bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen);

    if (status != 0) {
        syslog(LOG_ERR, "bind failed to bind socket: \n%s", strerror(errno));
        close(socket_fd);
        freeaddrinfo(servinfo);
        return -1;
    }

    if ((argc > 1) && (strcmp(argv[1], "-d") == 0)) {
        switch (pid = fork()) {
        case -1:
            // fork failed
            syslog(LOG_ERR, "Forked failed\n%s", strerror(errno));
            return -1;
            break;
        case 0:
            break;
        default:
            // parent
            // wait_pid = waitpid(pid, &wstatus, 0);

            // return_val = WIFEXITED(wstatus);
            // if (!return_val) {
            //     syslog(LOG_ERR, "Child did not exit succesfully\n%m");
            //     return -1;
            // }
            // else if (!(return_val &= (WEXITSTATUS(wstatus) != 1))) {
            //     syslog(LOG_ERR, "Exit Status: %d\n", WEXITSTATUS(wstatus));
            //     return -1;
            // }
            // else if (!(return_val &= wait_pid == pid)) {
            //     syslog(LOG_ERR, "Wait waited(%d) for the wrong pid(%d)", wait_pid, pid);
            //     return -1;
            // }
            // else if (!(return_val &= !(WIFSIGNALED(wstatus)))) {
            //     syslog(LOG_ERR, "Child did not exit succesfully was terminated by a signal\n%m");
            //     return -1;
            // }
            return 0;
        }
    }

    action.sa_handler = termination_handler;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1) {
        syslog(LOG_ERR, "Error setting up sigaction for SIGINT");
        return -1;
    }
    if (sigaction(SIGTERM, &action, NULL) == -1) {
        syslog(LOG_ERR, "Error setting up sigaction for SIGTERM");
        return -1;
    }
    status = listen(socket_fd, 10);
    if (status != 0) {
        syslog(LOG_ERR, "listen failed: \n%s", strerror(errno));
        close(socket_fd);
        freeaddrinfo(servinfo);
        return -1;
    }

    file_fd = open(FILE_PATH, O_CREAT | O_RDWR | O_APPEND, 0664);

    while (operate == 1) {
        syslog(LOG_WARNING, "Waiting for connection");
        accepted_socket_fd = accept(socket_fd, servinfo->ai_addr, &servinfo->ai_addrlen);

        if (accepted_socket_fd == -1) {
            if (operate == 0){
                syslog(LOG_INFO, "Closed connection from %s", ip_addr);
                close(accepted_socket_fd);
                free_buff_ll(node_base);
                node_base = NULL;
                break;
            }
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        inet_ntop(servinfo->ai_addr->sa_family, 
                //   servinfo->ai_addr->sa_data,
                  &(servinfo->ai_addr),
                  ip_addr, 
                  INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", ip_addr);

        node_base = create_buff_ll();
        cur_node = node_base;
        if (node_base == NULL) {
            syslog(LOG_ERR, "node_base failed");
            close(accepted_socket_fd);
            free_buff_ll(node_base);
            node_base = NULL;
            continue;
        }

        while ((operate == 1) && ((bytes = recv(accepted_socket_fd, cur_node->buff, BUFFER_SIZE -1, 0)) > 0)) {
            cur_node->buff[bytes] = '\0'; // NULL-terminate the received data
            while ((found_newline = strchr(cur_node->buff, '\n')) != NULL) {
                ssize_t out_len = found_newline - cur_node->buff + 1;
                write_out(file_fd, accepted_socket_fd, node_base, out_len);
                next_node = create_buff_ll();
                if (out_len < strlen(cur_node->buff)) {
                    memcpy(next_node->buff, found_newline + 1, strlen(found_newline +1));
                }
                free_buff_ll(node_base);
                node_base = next_node;
                cur_node = next_node;
            }
            cur_node = add_buff_ll(cur_node);
        }

        if ((bytes == 0) || (operate == 0)) {
            syslog(LOG_INFO, "Closed connection from %s", ip_addr);
            close(accepted_socket_fd);
            free_buff_ll(node_base);
            node_base = NULL;
            if (operate == 0) {
                break;
            }
            else {
                continue;
            }
        }
        else if (bytes == -1) {
            syslog(LOG_ERR, "recv failed: \n%s", strerror(errno));
            close(accepted_socket_fd);
            free_buff_ll(node_base);
            node_base = NULL;
            continue;
        }
    }




    close(socket_fd);
    close(file_fd);
    unlink(FILE_PATH);
    freeaddrinfo(servinfo);

    return 0;
}