
#include "main.h"

void saveProgLoad(int id, int v, sqlite3 *db) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set load=%d where id=%d", v, id);
    if (!db_exec(db, q, 0, 0)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "saveProgLoad(): query failed: %s\n", q);
#endif
    }
}

void saveProgLoadP(int id, int v, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgLoadP(): failed to open db: %s\n", db_path);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set load=%d where id=%d", v, id);
    if (!db_exec(db, q, 0, 0)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "saveProgLoadP(): query failed: %s\n", q);
#endif
    }
}

void saveProgEnable(int id, int v, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgEnable(): failed to open db: %s\n", db_path);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set enable=%d where id=%d", v, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgEnable(): query failed: %s\n", q);
    }
}

void saveProgSMS(int id, int value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgSMS(): failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set sms=%d where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgSMS(): query failed: %s\n", q);
    }
    sqlite3_close(db);
}

void saveProgRing(int id, int value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgRing(): failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set ring=%d where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgRing(): query failed: %s\n", q);
    }
    sqlite3_close(db);
}

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProg: ERROR: can not load prog with id=%d - list length exceeded\n", item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockProg(list->last);
        list->last->next = item;
        unlockProg(list->last);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("addProg: prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

int addProgById(int prog_id, ProgList *list) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "addProgById(): program with id = %d is being controlled by program\n", rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
        fputs("addProgById(): failed to allocate memory\n", stderr);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;
    strcpy(item->db_data_path, db_data_path);
    strcpy(item->db_public_path, db_public_path);
    strcpy(item->db_log_path, db_log_path);
    item->cycle_duration = cycle_duration;
    item->log_limit = log_limit;
    item->thread_data.cmd.ready = 0;
    item->thread_data.cmd.value = OFF;
    item->thread_data.state = INIT;
    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!initMutex(&item->thread_data.cmd.mutex)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!initClient(&item->sock_fd, WAIT_RESP_TIMEOUT)) {
        freeMutex(&item->thread_data.cmd.mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->thread_data.cmd.mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!checkProg(item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->thread_data.cmd.mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    printf("SIZE OF PROG: %d\n", sizeof *item);
    if (!addProg(item, list)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->thread_data.cmd.mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread_data.thread, &threadFunction, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->thread_data.cmd.mutex);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockProg(prev);
                prev->next = curr->next;
                unlockProg(prev);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            saveProgLoadP(curr->id, 0, curr->db_data_path);
            stopProgThread(curr);
            freeProg(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgList *list = d;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            int id = atoi(argv[i]);
            addProgById(id, list);
        } else {
            fputs("loadActiveProg_callback(): unknown column\n", stderr);
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ProgList *list, char *db_path) {
    sqlite3 *db;
    if (!db_openR(db_path, &db)) {
        return 0;
    }
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, list)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "loadActiveProg(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int callHuman(Prog *item, char *message, Peer *peer, const char *db_path) {
    S1List pn_list;
    if (!config_getPhoneNumberListO(&pn_list, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "callHuman: error while reading phone book\n");
#endif
        return 0;
    }
    for (int i = 0; i < pn_list.length; i++) {
        if (item->sms) {
            acp_sendSMS(peer, &pn_list.item[LINE_SIZE * i], message);
        }
        break;
    }
    for (int i = 0; i < pn_list.length; i++) {
        if (item->ring) {
            acp_makeCall(peer, &pn_list.item[LINE_SIZE * i]);
        }
    }
    FREE_LIST(&pn_list);
    return 1;
}

int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            item->id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("peer_id")) {
            if (!config_getPeer(&item->peer, argv[i], &item->sock_fd, NULL, item->db_public_path)) {
                return EXIT_FAILURE;
            }
        } else if (DB_COLUMN_IS("call_peer_id")) {
            if (!config_getPeer(&item->call_peer, argv[i], &item->sock_fd, NULL, item->db_public_path)) {
                return EXIT_FAILURE;
            }
        } else if (DB_COLUMN_IS("description")) {
            memcpy(item->description, argv[i], sizeof item->description);
        } else if (DB_COLUMN_IS("check_interval")) {
            item->check_interval.tv_nsec = 0;
            item->check_interval.tv_sec = atoi(argv[i]);
        } else if (DB_COLUMN_IS("cope_duration")) {
            item->cope_duration.tv_nsec = 0;
            item->cope_duration.tv_sec = atoi(argv[i]);
        } else if (DB_COLUMN_IS("phone_number_group_id")) {
            item->phone_number_group_id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("sms")) {
            item->sms = atoi(argv[i]);
        } else if (DB_COLUMN_IS("ring")) {
            item->ring = atoi(argv[i]);
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(argv[i]);
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(argv[i]);
        } else {
            fputs("loadProg_callback: unknown column\n", stderr);
        }
    }

    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if (!load) {
        saveProgLoad(item->id, 1, data->db);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item) {
    sqlite3 *db;
    if (!db_openR(db_data_path, &db)) {
        return 0;
    }
    char q[LINE_SIZE];
    ProgData data = {.db = db, .prog = item};
    snprintf(q, sizeof q, "select " PROG_FIELDS " from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "getProgByIdFDB(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}
