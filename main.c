#include "main.h"

int app_state = APP_INIT;

char db_data_path[LINE_SIZE];
char db_log_path[LINE_SIZE];
char db_public_path[LINE_SIZE];

char call_peer_id[NAME_SIZE];

int sock_port = -1;
int sock_fd = -1;
int log_limit = 0;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
I1List i1l;
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
ProgList prog_list = {NULL, NULL, 0};

#include "util.c"
#include "db.c"

int readSettings() {
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        perror("readSettings()");
#endif
        return 0;
    }
    char s[LINE_SIZE];
    fgets(s, LINE_SIZE, stream);
    int n;
    n = fscanf(stream, "%d\t%ld\t%ld\t%d\t%255s\t%255s\t%255s\n",
            &sock_port,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            &log_limit,
            db_data_path,
            db_public_path,
            db_log_path

            );
    if (n != 7) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: \n\tsock_port: %d, \n\tcycle_duration: %ld sec %ld nsec, \n\tlog_limit: %d, \n\tdb_data_path: %s, \n\tdb_public_path: %s, \n\tdb_log_path: %s\n", sock_port, cycle_duration.tv_sec, cycle_duration.tv_nsec, log_limit, db_data_path, db_public_path, db_log_path);
#endif
    return 1;
}

int initData() {
    if (!loadActiveProg(&prog_list, db_data_path)) {
        freeProgList(&prog_list);
        return 0;
    }
    if (!initI1List(&i1l, ACP_BUFFER_MAX_SIZE)) {
        freeProgList(&prog_list);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS

    if (
            ACP_CMD_IS(ACP_CMD_PROG_STOP) ||
            ACP_CMD_IS(ACP_CMD_PROG_START) ||
            ACP_CMD_IS(ACP_CMD_PROG_RESET) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)
            ) {
        acp_requestDataToI1List(&request, &i1l);
        if (i1l.length <= 0) {
            return;
        }

    } else {
        return;
    }
    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list);
            }
        }
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    }
    acp_responseSend(&response, &peer_client);

}
#define INTERVAL_CHECK if (!ton_ts(item->check_interval, &item->tmr_check)){return;}acp_pingPeer(&item->peer);

void progControl(Prog *item) {
#ifdef MODE_DEBUG
    char *state = getStateStr(item->state);
    struct timespec tm1 = getTimeRestCheck(item);
    struct timespec tm2 = getTimeRestCope(item);
    printf("progControl: prog_id=%d state=%s peer_id=%s peer_active=%d check_time_rest=%ldsec alert_time_rest=%ldsec\n", item->id, state, item->peer.id, item->peer.active, tm1.tv_sec, tm2.tv_sec);
#endif
    switch (item->state) {
        case INIT:
            ton_ts_reset(&item->tmr_check);
            ton_ts_reset(&item->tmr_cope);
            item->g_count = 0;
            item->state = WBAD;
            break;
        case WBAD:
            INTERVAL_CHECK
            if (!item->peer.active) {
                item->state = WCOPE;
            }
            break;
        case WCOPE:
            INTERVAL_CHECK
            if (item->peer.active) {
                item->tmr_cope.ready = 0;
                item->g_count = 0;
                item->state = WBAD;
            }
            if (ton_ts(item->cope_duration, &item->tmr_cope)) {
                char msg[LINE_SIZE];
                snprintf(msg, sizeof msg, "check system: %s", item->description);
                log_saveAlert(msg, item->log_limit, item->db_log_path);
                callHuman(item, msg, &item->call_peer, item->db_public_path);
                item->g_count = 0;
                item->state = WGOOD;
            }
            break;
        case WGOOD:
            INTERVAL_CHECK
            if (item->peer.active) {
                item->g_count++;
            }
            if (item->g_count >= GOOD_COUNT) {
                item->g_count = 0;
                item->state = WBAD;
            }
            break;
        case DISABLE:
            item->state = OFF;
            break;
        case OFF:
            break;
        default:
            item->state = INIT;
            break;
    }


}
#undef INTERVAL_CHECK

void *threadFunction(void *arg) {
    Prog *item = arg;
#ifdef MODE_DEBUG
    printf("thread for program with id=%d has been started\n", item->id);
#endif
    while (1) {
#ifdef MODE_DEBUG
        char *thread_state = getStateStr(item->thread_data.state);
        printf("thread_%d state:%s\n", item->id, thread_state);
#endif
        struct timespec t1 = getCurrentTime();
        switch (item->thread_data.state) {
            case INIT:
                item->thread_data.state = RUN;
                break;
            case RUN:
            {
                int old_state;
                if (threadCancelDisable(&old_state)) {
                    if (lockMutex(&item->mutex)) {
                        progControl(item);
                        unlockMutex(&item->mutex);
                    }
                    threadSetCancelState(old_state);
                }
                break;
            }
            case STOP:
                return EXIT_SUCCESS;

            case FAILURE:
                break;
        }

        /*
                if (tryLockMutex(&item->thread_data.cmd.mutex)) {
                    if (item->thread_data.cmd.ready) {
                        switch (item->thread_data.cmd.value) {
                            case RESET:
                            case STOP:
                                item->thread_data.state = STOP;
                                break;
                            default:
                                break;
                        }
                        item->thread_data.cmd.ready = 0;
                    }
                    unlockMutex(&item->thread_data.cmd.mutex);
                }
         */
        pthread_testcancel();
        sleepRest(item->cycle_duration, t1);
    }
}

void freeData() {
#ifdef MODE_DEBUG

    puts("freeData:");
#endif
    stopAllProgThreads(&prog_list);
    FREE_LIST(&i1l);
    freeProgList(&prog_list);
#ifdef MODE_DEBUG
    puts(" done");
#endif
}

void freeApp() {

    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
}

void exit_nicely() {

    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {

    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}