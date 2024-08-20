#ifndef HTTP_H
#define HTTP_H

struct http_request {
    int priority;
    char *method;
    char *path;
    int delay;
    char *work;
};

#endif