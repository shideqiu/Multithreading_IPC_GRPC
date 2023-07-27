#include "proxy-student.h"
#include "gfserver.h"

#define MAX_REQUEST_N 822
#define BUFSIZE (1024)

/*
 * Replace with an implementation of handle_with_curl and
 * any other functions you may need...
 */

ssize_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {

    size_t bytes_written = size * nmemb;
    return gfs_send(userdata, ptr, bytes_written);
}

ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
	(void) ctx;
	(void) arg;
	(void) path;
	/* not implemented */
    int content_length = 0;
    char url[1024];
//    char *host = arg;
    strcpy(url, arg);
    strcat(url, path);
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "curl_easy_init failed\n");
        return SERVER_FAILURE;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return SERVER_FAILURE;
    }
    int res_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
    if (res_code != 200) {
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        curl_easy_cleanup(curl);
        return 0;
    }
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
    gfs_sendheader(ctx, GF_OK, content_length);
    curl_easy_cleanup(curl);

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "curl_easy_init failed\n");
        return SERVER_FAILURE;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return SERVER_FAILURE;
    }
	errno = ENOSYS;
    curl_easy_cleanup(curl);
	return 0;
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl
 * as a convenience for linking.  We recommend you simply modify the proxy to
 * call handle_with_curl directly...
 */
