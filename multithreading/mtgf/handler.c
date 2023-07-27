#include "gfserver-student.h"
#include "workload.h"
#include "gfserver.h"
#include "content.h"

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//
pthread_mutex_t m_server = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_server = PTHREAD_COND_INITIALIZER;
steque_t *queue;

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
	// not yet implemented.
    steque_item_info *item_info = malloc(sizeof(steque_item_info));
    item_info->context = *ctx;
    item_info->filepath = path;

    pthread_mutex_lock(&m_server);
    steque_enqueue(queue, item_info);
    printf("Add a new item in to the queue\n");
    printf("The item path is %s\n", path);
    printf("The content is %s\n", (char *)*ctx);
    pthread_mutex_unlock(&m_server);

    pthread_cond_signal(&cond_server);
    (*ctx) = NULL;
	return gfh_success;
}

