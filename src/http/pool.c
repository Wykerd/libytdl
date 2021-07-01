// TODO NULL CHECKS !!
#include "img-panda/http/pool.h"
#include <string.h>

static void imp__wc_on_close_idle (uv_handle_t* handle);

static void imp__error_close_async (uv_handle_t *handle) {

    free(handle);
}

static void imp__error_send_async (uv_async_t* handle) {
    imp_http_worker_t *state = handle->data;
    if (state->on_error != NULL) 
        state->on_error(state, &state->last_error);
    else
        state->pool->on_error_default(state, &state->last_error);

    uv_close((uv_handle_t *)handle, imp__error_close_async);
}

inline
static void imp__http_pool_kill_worker (imp_http_worker_t *state, imp_net_status_t *err) {
    printf("!!! KILLING WORKER %p. WORKER IDLE %d. WORKER URL: %s. ERROR: %d\n", state, state->is_idle, state->client.url->path, err->type);
    state->is_connected = 0;

    memcpy(&state->last_error, err, sizeof(imp_net_status_t));

    int was_active = !state->is_idle;
    
    imp_http_client_shutdown(&state->client, imp__wc_on_close_idle);

    // is not idle some error caused an issue
    if (was_active) {
        uv_async_t *err_async = malloc(sizeof(uv_async_t));
        uv_async_init(state->pool->loop, err_async, imp__error_send_async);
        err_async->data = state;
        uv_async_send(err_async);
    }
}

static void imp__http_pool_write_cb (imp_http_client_t *client, imp_net_status_t *err) {
    printf("*** WORKER %p SENT REQUEST\n", client->data);
    imp_http_worker_t *state = client->data;
    if (err->type != FA_NET_E_OK) {
        imp__http_pool_kill_worker(state, err);
    };
    imp_http_request_serialize_free(state->last_request_buf);
    state->last_request_buf = NULL;
};

static int imp__wc_on_status_recv (llhttp_t* parser, const char *at, size_t length) {
    imp_http_worker_t *state = (imp_http_worker_t *)((imp_http_client_t *)(parser->data))->data;
    state->should_redirect = ((parser->status_code > 299) && (parser->status_code < 400));
    if (!state->should_redirect) {
        if (!((parser->status_code > 199) && (parser->status_code < 300))) {
            imp_net_status_t err = {
                .type = FA_NET_NOT_2XX,
                .code = parser->status_code 
            };
            imp__http_pool_kill_worker(state, &err);
        };
    };

    return 0;
}

static void imp__http_pool_ready_cb (imp_http_client_t *client) {
    imp_http_worker_t *state = client->data;
    printf("*** WORKER %p CONNECTED IN POOL\n", state);
    state->is_connected = 1;
    state->last_request_buf = imp_http_request_serialize_with_url(state->last_request, state->last_request_body, state->client.url);
    imp_http_client_write(client, state->last_request_buf, imp__http_pool_write_cb);
}

static void imp__http_pool_status_cb (imp_http_client_t *client, imp_net_status_t *err) {
    imp_http_worker_t *state = client->data;
    if (err->type != FA_NET_E_OK) {
        imp__http_pool_kill_worker(state, err);
    }
};

int imp__wc_on_body_recv (llhttp_t* parser, const char *at, size_t length) {
    imp_http_worker_t * state = (imp_http_worker_t *)((imp_http_client_t *)(parser->data))->data;
    
    size_t new_size = state->last_response.len + length;

    if (new_size > state->last_response.size) {
        state->last_response.size = new_size;
        state->last_response.base = realloc(state->last_response.base, new_size);
    };

    memcpy(state->last_response.base + state->last_response.len, at, length);
    state->last_response.len += length;

    return 0;
};

int imp__wc_on_header_field (llhttp_t* parser, const char *at, size_t length) {
    imp_http_worker_t * state = (imp_http_worker_t *)((imp_http_client_t *)(parser->data))->data;
    if (state->should_redirect && (length == 8))
        state->is_location_header = imp_http_header_is_equal(at, length, "location", 8);
    else
        state->is_location_header = 0;

    return 0;
}

int imp__wc_on_header_value (llhttp_t* parser, const char *at, size_t length) {
    imp_http_worker_t * state = (imp_http_worker_t *)((imp_http_client_t *)(parser->data))->data;
    if (state->is_location_header) {
        state->redirect_url = imp_parse_url(at, length);
        if (state->redirect_url == NULL) {
            // TODO: EXIT AND SHUTDOWN
        } else 
            state->redirect_new_host = imp_url_same_host(state->redirect_url, state->client.url);
    }

    return 0;
}

static void imp__wc_on_close_redirect (uv_handle_t* handle) {
    imp_http_worker_t *state = (imp_http_worker_t *)((imp_http_client_t *)handle->data)->data;
    printf("<<< Redirecting to: %s\n\n", state->redirect_url->host);
    imp_url_free(state->client.url);
    imp_http_client_init(handle->loop, &state->client);
    state->client.url = state->redirect_url;
    if (imp_http_client_connect (&state->client, imp__http_pool_status_cb, imp__http_pool_ready_cb)) {
        // TODO: EXIT NO SHUTDOWN
    };
};

static void imp__pool_on_close_new_host (uv_handle_t* handle) {
    imp_http_worker_t *state = (imp_http_worker_t *)((imp_http_client_t *)handle->data)->data;
    imp_http_client_init(handle->loop, &state->client);
    state->client.url = state->redirect_url;
    if (imp_http_client_connect (&state->client, imp__http_pool_status_cb, imp__http_pool_ready_cb)) {
        // TODO: EXIT NO SHUTDOWN
    };
};

inline 
static int imp__http_pool_set_working (imp_http_pool_t *pool, imp_http_worker_t *idle) {
    printf("*** WORKER %p MOVED TO WORK POOL\n", idle);
    pool->working_workers.workers[idle->pos] = idle;
    pool->working_workers.workers[idle->pos]->is_idle = 0;
    pool->idle_workers.workers[idle->pos] = NULL;
    pool->idle_workers.len--;
    pool->working_workers.len++;
    if (pool->on_state_change != NULL)
        pool->on_state_change(pool->working_workers.workers[idle->pos], pool);
    return 1;
}

inline 
static imp_http_worker_t *imp__http_pool_set_idle (imp_http_pool_t *pool, imp_http_worker_t *work) {
    printf("*** WORKER %p MOVED TO IDLE POOL\n", work);
    pool->idle_workers.workers[work->pos] = work;
    pool->idle_workers.workers[work->pos]->is_idle = 1;
    pool->working_workers.workers[work->pos] = NULL;
    pool->working_workers.len--;
    pool->idle_workers.len++;
    if (pool->on_state_change != NULL)
        pool->on_state_change(pool->idle_workers.workers[work->pos], pool);
    return pool->idle_workers.workers[work->pos];
}

static void imp__wc_on_close_idle (uv_handle_t* handle) {
    imp_http_worker_t *state = (imp_http_worker_t *)((imp_http_client_t *)handle->data)->data;
    imp_http_client_init(state->pool->loop, &state->client);
    if (!state->is_idle) {
        if (state->last_request != NULL)
            imp_http_request_free(state->last_request);
        imp__http_pool_set_idle(state->pool, state);
    }
}

int imp__pool_on_message_complete (llhttp_t* parser) {
    imp_http_worker_t * state = (imp_http_worker_t *)((imp_http_client_t *)(parser->data))->data;
    
    // Handle redirects
    if (state->should_redirect) {
        if (state->redirect_new_host) 
            imp_http_client_shutdown (&state->client, imp__wc_on_close_redirect);
        else {
            printf("<<< Redirecting to: %s\n<<< Using existing connection!\n", state->redirect_url->host);
            imp_url_free(state->client.url);
            state->client.url = state->redirect_url;
            imp__http_pool_ready_cb(parser->data);
        }
        state->redirect_new_host = 0;
        state->should_redirect = 0;
        goto cleanup;
    };

    // Process the response
    if (state->on_response != NULL) {
        state->on_response(state, state->pool);
    };

    // Check if we can consume one of the queue items
    if (state->pool->queue_len > 0) {
        printf("*** WORKER %p CONSUMING QUEUED REQUEST\n", state);
        imp_http_worker_request_t *req = state->pool->queue[state->pool->queue_len - 1];

        imp_http_request_free(state->last_request);
        state->last_request = req->request;
        state->last_request_data = req->data;
        state->on_error = req->on_error;
        state->on_response = req->on_response;
        state->last_request_body = req->body;

        if (!strcmp(req->url->host, state->client.url->host)) {
            // has same host
            imp_url_free(state->client.url);
            state->client.url = req->url;
            imp__http_pool_ready_cb(&state->client);
        } else {
            // not same host close and reconnect
            imp_url_free(state->client.url);
            state->redirect_url = req->url;
            imp_http_client_shutdown(&state->client, imp__pool_on_close_new_host);
        }

        free(req);

        state->pool->queue[state->pool->queue_len - 1] = NULL;
        state->pool->queue_len--;
        goto cleanup;
    }
    // free the request - we have handled it now
    imp_http_request_free(state->last_request);
    state->last_request = NULL;
    // reset the buffer
    state->last_response.len = 0;

    state = imp__http_pool_set_idle(state->pool, state);

    return 0;
cleanup:
    // reset the buffer
    state->last_response.len = 0;

    return 0;
};

int imp_http_worker_init (imp_http_pool_t *pool, imp_http_worker_t *worker) {
    memset(worker, 0, sizeof(imp_http_worker_t));
    imp_http_client_init(pool->loop, &worker->client);

    worker->client.parser_settings.on_body = *imp__wc_on_body_recv;
    worker->client.parser_settings.on_message_complete = *imp__pool_on_message_complete;
    worker->client.parser_settings.on_status = *imp__wc_on_status_recv;
    worker->client.parser_settings.on_header_field = *imp__wc_on_header_field;
    worker->client.parser_settings.on_header_value = *imp__wc_on_header_value;

    worker->client.settings.keep_alive = 1;
    worker->client.data = worker;

    worker->is_idle = 1;

    worker->pool = pool;

    // set default response buffer size to the default https buffer size (8KiB)
    worker->last_response.base = calloc(sizeof(char), FA_HTTPS_BUF_LEN);
    worker->last_response.len = 0;
    worker->last_response.size = FA_HTTPS_BUF_LEN;

    return 1;
}

static int imp__http_pool_queue_grow (imp_http_pool_t *pool) {
    size_t old_size = pool->queue_size;
    pool->queue_size *= 1.5; // growth factor
    pool->queue = realloc(pool->queue, sizeof(imp_http_worker_request_t *) * pool->queue_size);

    if (pool->queue == NULL)
        return 0;
    
    return 1;
}

int imp_http_pool_init (uv_loop_t *loop, imp_http_pool_t *pool, size_t worker_count) {
    memset(pool, 0, sizeof(imp_http_pool_t));

    pool->loop = loop;
    pool->pool_size = worker_count;

    pool->queue_size = worker_count;
    pool->queue = malloc(sizeof(imp_http_worker_request_t *) * pool->queue_size);
    if (pool->queue == NULL)
        return 0;
    
    pool->queue_len = 0;

    pool->idle_workers.len = worker_count;
    pool->working_workers.len = 0;

    pool->idle_workers.workers = malloc(sizeof(imp_http_worker_t *) * worker_count);

    pool->working_workers.workers = malloc(sizeof(imp_http_worker_t *) * worker_count);

    for (size_t i = 0; i < worker_count; i++) {
        pool->idle_workers.workers[i] = malloc(sizeof(imp_http_worker_t));
        if (pool->idle_workers.workers[i] == NULL)
            return 0;
        imp_http_worker_init(pool, pool->idle_workers.workers[i]);
        pool->idle_workers.workers[i]->pos = i;
    };

    return 1;
};

int imp_http_pool_request (imp_http_pool_t *pool, imp_http_worker_request_t *request) {
    if (pool->idle_workers.len) {
        for (size_t i = 0; i < pool->pool_size; i++) {
            if ((pool->idle_workers.workers[i] != NULL) &&
                pool->idle_workers.workers[i]->is_connected && 
                !strcmp(request->url->host, pool->idle_workers.workers[i]->client.url->host)) 
            {
                // request can be submitted immediately
                pool->idle_workers.workers[i]->last_request = request->request;
                pool->idle_workers.workers[i]->last_request_data = request->data;
                pool->idle_workers.workers[i]->on_error = request->on_error;
                pool->idle_workers.workers[i]->on_response = request->on_response;
                if (pool->idle_workers.workers[i]->client.url != NULL)
                    imp_url_free(pool->idle_workers.workers[i]->client.url);

                pool->idle_workers.workers[i]->client.url = request->url;
                pool->idle_workers.workers[i]->last_request_body = request->body;

                pool->idle_workers.workers[i]->last_request_buf = imp_http_request_serialize_with_url(request->request, request->body, request->url);

                imp_http_client_write(&pool->idle_workers.workers[i]->client, pool->idle_workers.workers[i]->last_request_buf, imp__http_pool_write_cb);

                free(request);

                return imp__http_pool_set_working(pool, pool->idle_workers.workers[i]);
            };
        };

        for (size_t i = 0; i < pool->pool_size; i++) {
            if ((pool->idle_workers.workers[i] != NULL) && 
                (pool->idle_workers.workers[i]->is_connected == 0)) 
            {
                // is not connected
                pool->idle_workers.workers[i]->last_request = request->request;
                pool->idle_workers.workers[i]->last_request_data = request->data;
                pool->idle_workers.workers[i]->on_error = request->on_error;
                pool->idle_workers.workers[i]->on_response = request->on_response;

                if (pool->idle_workers.workers[i]->client.url != NULL)
                    imp_url_free(pool->idle_workers.workers[i]->client.url);

                pool->idle_workers.workers[i]->client.url = request->url;
                pool->idle_workers.workers[i]->last_request_body = request->body;

                imp_http_client_connect(&pool->idle_workers.workers[i]->client, imp__http_pool_status_cb, imp__http_pool_ready_cb);

                free(request);
                
                return imp__http_pool_set_working(pool, pool->idle_workers.workers[i]);
            };
        };

        for (size_t i = 0; i < pool->pool_size; i++) {
            if (pool->idle_workers.workers[i] != NULL) 
            {
                pool->idle_workers.workers[i]->last_request = request->request;
                pool->idle_workers.workers[i]->last_request_data = request->data;
                pool->idle_workers.workers[i]->on_error = request->on_error;
                pool->idle_workers.workers[i]->on_response = request->on_response;

                if (pool->idle_workers.workers[i]->client.url != NULL)
                    imp_url_free(pool->idle_workers.workers[i]->client.url);

                pool->idle_workers.workers[i]->redirect_url = request->url;

                imp_http_client_shutdown (&pool->idle_workers.workers[i]->client, imp__pool_on_close_new_host);

                free(request);

                return imp__http_pool_set_working(pool, pool->idle_workers.workers[i]);
            }
        };
        return 0;
    } else {
        if (pool->queue_len >= pool->queue_size)
            imp__http_pool_queue_grow(pool);
        pool->queue[pool->queue_len] = request;
        pool->queue_len++;
        return 1;
    };
};

static void imp__pool_on_close_sht (uv_handle_t* handle) {
    imp_http_worker_t *state = (imp_http_worker_t *)((imp_http_client_t *)handle->data)->data;

    if (state->last_request_buf != NULL) 
        imp_http_request_serialize_free(state->last_request_buf);

    free(state->last_response.base);

    imp_url_free(state->redirect_url);
    free(state);
}

void imp_http_pool_shutdown (imp_http_pool_t *pool) {
    for (size_t i = 0; i < pool->pool_size; i++) {
        if (pool->idle_workers.workers[i] != NULL) {
            imp_http_client_t *client = &pool->idle_workers.workers[i]->client;
            imp_http_client_shutdown(&pool->idle_workers.workers[i]->client, imp__pool_on_close_sht);
        } else if (pool->working_workers.workers[i] != NULL) {
            imp_http_client_shutdown(&pool->working_workers.workers[i]->client, imp__pool_on_close_sht);
        }
    };

    for (size_t i = 0; i < pool->queue_len; i++) {
        imp_http_request_free(pool->queue[i]->request);
        imp_url_free(pool->queue[i]->url);
        free(pool->queue[i]);
    };

    free(pool->queue);

    free(pool->idle_workers.workers);
    free(pool->working_workers.workers);
};
