/*
 *  To be used by students
 */
 #ifndef __CACHE_STUDENT_H__824
 #define __CACHE_STUDENT_H__824




 #endif // __CACHE_STUDENT_H__824
#include "steque.h"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <mqueue.h>
#include <semaphore.h>
#include<sys/msg.h>

struct message {
    int id;
    int type;
    char path[256];
    char content[824];
    int file_len;
    int content_len;
};