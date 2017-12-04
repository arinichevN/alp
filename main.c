/*
 * alp
 */

#include "main.h"

char pid_path[LINE_SIZE];

int app_state = APP_INIT;

char db_data_path[LINE_SIZE];
char db_log_path[LINE_SIZE];
char db_public_path[LINE_SIZE];

char call_peer_id[NAME_SIZE];
Peer *call_peer = NULL;

int pid_file = -1;
int proc_id;
int sock_port = -1;
int sock_fd = -1;
int sock_fd_tf = -1;

unsigned int log_limit = 0;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
DEF_THREAD
        struct timespec rsens_interval_min = {1, 0};
struct timespec peer_ping_interval = {3, 0};
I1List i1l = {NULL, 0};
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
PeerList peer_list = {NULL, 0};
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
    n = fscanf(stream, "%d\t%255s\t%ld\t%ld\t%u\t%32s\t%255s\t%255s\t%255s\n",
            &sock_port,
            pid_path,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            &log_limit,
            call_peer_id,
            db_data_path,
            db_public_path,
            db_log_path

            );
    if (n != 9) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: \n\tsock_port: %d, \n\tpid_path: %s, \n\tcycle_duration: %ld sec %ld nsec, \n\tlog_limit: %u, \n\tcall_peer_id: %s, \n\tdb_data_path: %s, \n\tdb_public_path: %s, \n\tdb_log_path: %s\n", sock_port, pid_path, cycle_duration.tv_sec, cycle_duration.tv_nsec, log_limit, call_peer_id, db_data_path, db_public_path, db_log_path);
#endif
    return 1;
}

int initData() {
    if (!config_getPeerList(&peer_list, &sock_fd_tf, db_public_path)) {
        FREE_LIST(&peer_list);
        return 0;
    }
    call_peer = getPeerById(call_peer_id, &peer_list);
    if (call_peer == NULL) {
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!loadActiveProg(db_data_path, &prog_list, &peer_list)) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!initI1List(&i1l, ACP_BUFFER_MAX_SIZE)) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!THREAD_CREATE) {
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }

    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }

    if (!initClient(&sock_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
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
        acp_requestDataToI1List(&request, &i1l, prog_list.length);
        if (i1l.length <= 0) {
            return;
        }

    } else {
        return;
    }
    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &peer_list, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                curr->state = OFF;
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &peer_list, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    if (!bufCatProgInit(curr, &response)) {
                        unlockProg(curr);
                        return;
                    }
                    unlockProg(curr);
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    if (!bufCatProgInit(curr, &response)) {
                        unlockProg(curr);
                        return;
                    }
                    unlockProg(curr);
                }
            }
        }
    }
    acp_responseSend(&response, &peer_client);

}
#define INTERVAL_CHECK if (!ton_ts(item->check_interval, &item->tmr_check)){return;}acp_pingPeer(&item->peer);

void progControl(Prog *item) {

    switch (item->state) {
        case INIT:
            item->tmr_check.ready = 0;
            item->tmr_cope.ready = 0;
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
                log_saveAlert(msg, log_limit, db_log_path);
                callHuman(item, msg, call_peer, db_public_path);
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
#ifdef MODE_DEBUG
    char *state = getStateStr(item->state);
    struct timespec tm1 = getTimeRestCheck(item);
    struct timespec tm2 = getTimeRestCope(item);
    printf("progControl: prog_id=%d state=%s peer_id=%s peer_active=%d check_time_rest=%ldsec alert_time_rest=%ldsec\n", item->id, state, item->peer.id, item->peer.active, tm1.tv_sec, tm2.tv_sec);
#endif

}

void *threadFunction(void *arg) {
    THREAD_DEF_CMD
#ifdef MODE_DEBUG
            puts("threadFunction: running...");
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        lockProgList();
        Prog *curr = prog_list.top;
        unlockProgList();
        while (1) {
            if (curr == NULL) {
                break;
            }

            if (tryLockProg(curr)) {
                progControl(curr);
                Prog *temp = curr;
                curr = curr->next;
                unlockProg(temp);
            }

            THREAD_EXIT_ON_CMD
        }
        THREAD_EXIT_ON_CMD
        sleepRest(cycle_duration, t1);
    }
}

void freeProg(ProgList *list) {
    Prog *curr = list->top, *temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void freeData() {
#ifdef MODE_DEBUG
    puts("freeData:");
#endif
    THREAD_STOP
    FREE_LIST(&i1l);
    freeProg(&prog_list);
    FREE_LIST(&peer_list);
#ifdef MODE_DEBUG
    puts(" done");
#endif
}

void freeApp() {
#ifdef MODE_DEBUG
    puts("freeApp:");
#endif
    freeData();
#ifdef MODE_DEBUG
    puts(" freeData: done");
#endif
    freeSocketFd(&sock_fd);
#ifdef MODE_DEBUG
    puts(" free sock_fd: done");
#endif
    freeSocketFd(&sock_fd_tf);
#ifdef MODE_DEBUG
    puts(" sock_fd_tf: done");
#endif
    freePid(&pid_file, &proc_id, pid_path);
#ifdef MODE_DEBUG
    puts(" freePid: done");
#endif
#ifdef MODE_DEBUG
    puts(" done");
#endif
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