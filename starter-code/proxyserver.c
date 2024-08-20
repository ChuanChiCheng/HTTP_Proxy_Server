#include <pthread.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "safequeue.h"
#include "proxyserver.h"

#define BUFFER_SIZE 1024
#define ERROR_QUEUE_EMPTY 404

/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
int *listener_ports;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;
SafeQueue* safeQueue;

void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(QueueElement server) {
    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    // fileserver_fd = set_socket_non_blocking(fileserver_fd);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(server.client_fd, BAD_GATEWAY, "Bad Gateway");
        return;
    }

    // successfully connected to the file server
    char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));

    // forward the client request to the fileserver
    int ret = http_send_data(fileserver_fd, server.request.work, strlen(server.request.work));

    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(server.client_fd, BAD_GATEWAY, "Bad Gateway");
    }

    // forward the fileserver response to the client
    while (1) {
        int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
        if (bytes_read <= 0) // fileserver_fd has been closed, break
            break;
        ret = http_send_data(server.client_fd, buffer, bytes_read);
        if (ret < 0) { // write failed, client_fd has been closed
            break;
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // Free resources and exit
    free(buffer);
}


int server_fd;
/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
// void serve_forever(int *server_fd) {

//     // create a socket to listen
//     *server_fd = socket(PF_INET, SOCK_STREAM, 0);
//     if (*server_fd == -1) {
//         perror("Failed to create a new socket");
//         exit(errno);
//     }

//     // manipulate options for the socket
//     int socket_option = 1;
//     if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
//                    sizeof(socket_option)) == -1) {
//         perror("Failed to set socket options");
//         exit(errno);
//     }


//     int proxy_port = listener_ports[0];
//     // create the full address of this proxyserver
//     struct sockaddr_in proxy_address;
//     memset(&proxy_address, 0, sizeof(proxy_address));
//     proxy_address.sin_family = AF_INET;
//     proxy_address.sin_addr.s_addr = INADDR_ANY;
//     proxy_address.sin_port = htons(proxy_port); // listening port

//     // bind the socket to the address and port number specified in
//     if (bind(*server_fd, (struct sockaddr *)&proxy_address,
//              sizeof(proxy_address)) == -1) {
//         perror("Failed to bind on socket");
//         exit(errno);
//     }

//     // starts waiting for the client to request a connection
//     if (listen(*server_fd, 1024) == -1) {
//         perror("Failed to listen on socket");
//         exit(errno);
//     }

//     printf("Listening on port %d...\n", proxy_port);

//     struct sockaddr_in client_address;
//     size_t client_address_length = sizeof(client_address);
//     int client_fd;
//     while (1) {
//         client_fd = accept(*server_fd,
//                            (struct sockaddr *)&client_address,
//                            (socklen_t *)&client_address_length);
//         if (client_fd < 0) {
//             perror("Error accepting socket");
//             continue;
//         }

//         printf("Accepted connection from %s on port %d\n",
//                inet_ntoa(client_address.sin_addr),
//                client_address.sin_port);

//         QueueElement server = parse_client_request(client_fd);
//         serve_request(client_fd);

//         // close the connection to the client
//         shutdown(client_fd, SHUT_WR);
//         close(client_fd);
//     }

//     shutdown(*server_fd, SHUT_RDWR);
//     close(*server_fd);
// }

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

void handle_getjob_request(int client_fd, SafeQueue *queue) {
    QueueElement job = get_work_nonblocking(queue);
    if (job.priority == -1) {
        send_error_response(client_fd,598,"empty queue");
        close(client_fd);
    } else {
        char *response = format_http_response(job.request);
        send(client_fd, response, strlen(response), 0);
        free(response);
    }
}

char *format_http_response(struct http_request request) {
    int response_size = strlen(request.path) + 100;
    char *response = malloc(response_size);
    if (response) {
        snprintf(response, response_size, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n%s", strlen(request.path), request.path);
    }
    return response;
}

void *listener_thread(void *arg) {
    int port = *((int *)arg);

    // Socket setup
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Cannot create socket");
        return NULL;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 1024) < 0) {
        perror("Listen failed");
        close(server_fd);
        return NULL;
    }


    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("Listener accept failed");
            continue;
        }

        struct http_request* request;
        request = parse_client_request(client_fd);

        if (strcmp(request->path, "/GetJob") == 0) {
            handle_getjob_request(client_fd, safeQueue);
        } else {
            QueueElement element = {.client_fd = client_fd, .priority = request->priority, .request = *request};
            if (add_work(safeQueue, element) == 0) {
                send_error_response(client_fd, 599, "Service Unavailable");
                close(client_fd);
            }
        }
    }

    close(server_fd);
    return NULL;
}

void *worker_thread(void *arg) {
    SafeQueue *queue = (SafeQueue *)arg;

    while (1) {
        if (queue == NULL) {
            return NULL;
        } else {
            QueueElement element = get_work(queue);
            int delay = element.request.delay;
            if (delay > 0) {
                sleep(delay);
            }
            serve_request(element);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);

    default_settings();

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
            }
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }
    print_settings();

    safeQueue = create_queue(max_queue_size);
    pthread_t listener_threads[num_listener];
    for (int i = 0; i < num_listener; i++) {
        pthread_create(&listener_threads[i], NULL, listener_thread, &listener_ports[i]);
    }

    pthread_t worker_threads[num_workers];
    printf("num_workers = %d\n", num_workers);
    for (int i = 0; i < num_workers; i++) {
        printf("%d\n", i);
        pthread_create(&worker_threads[i], NULL, worker_thread, (void *)safeQueue);
    }


   for (int i = 0; i < num_listener; i++) {
        pthread_join(listener_threads[i], NULL);
    }

    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    return EXIT_SUCCESS;
}