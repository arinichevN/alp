/*
 * alp
 */

#include "main.h"

FUN_LLIST_GET_BY_ID(Prog)

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

int lockProg(Prog *item) {
    if (pthread_mutex_lock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProg: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProg(Prog *item) {
    if (pthread_mutex_trylock(&(item->mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProg(Prog *item) {
    if (pthread_mutex_unlock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProg: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

int checkProg(const Prog *item, const ProgList *list) {

    if (item->check_interval.tv_sec <= 0 && item->check_interval.tv_nsec <= 0) {
        fprintf(stderr, "checkProg: bad check_interval where prog id = %d\n", item->id);
        return 0;
    }
    if (item->cope_duration.tv_sec < 0 && item->cope_duration.tv_nsec < 0) {
        fprintf(stderr, "checkProg: bad cope_duration where prog id = %d\n", item->id);
        return 0;
    }
    //unique id
    if (getProgById(item->id, list) != NULL) {
        fprintf(stderr, "checkProg: prog with id = %d is already running\n", item->id);
        return 0;
    }
    return 1;
}

struct timespec getTimeRestL(struct timespec interval, Ton_ts tmr) {
    struct timespec out = {-1, -1};
    if (tmr.ready) {
        out = getTimeRest_ts(interval, tmr.start);
    }
    return out;
}

struct timespec getTimeRestCope(const Prog *item) {
    return getTimeRestL(item->cope_duration, item->tmr_cope);
}

struct timespec getTimeRestCheck(const Prog *item) {
    return getTimeRestL(item->check_interval, item->tmr_check);
}

char * getStateStr(char state) {
    switch (state) {
        case OFF:
            return "OFF";
        case INIT:
            return "INIT";
        case WBAD:
            return "WBAD";
        case WCOPE:
            return "WCOPE";
        case WGOOD:
            return "WGOOD";
        case DISABLE:
            return "DISABLE";
    }
    return "\0";
}

int bufCatProgRuntime(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    char *state = getStateStr(item->state);
    struct timespec tm_rest = getTimeRestCope(item);
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
            item->id,
            state,
            tm_rest.tv_sec
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatProgInit(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
            item->id,
            item->check_interval.tv_sec,
            item->cope_duration.tv_sec,
            item->cell_peer.id,
            item->phone_number_group_id
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int sendStrPack(char qnf, char *cmd) {
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd,  &peer_client);
}

int sendBufPack(char *buf, char qnf, char *cmd_str) {
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str,  &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}

void waitThread_ctl(char cmd) {
    thread_cmd = cmd;
    pthread_join(thread, NULL);
}

void printAll() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    size_t i;
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "pid_path: %s\n", pid_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "sock_buf_size: %d\n", sock_buf_size);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "log_limit: %d\n", log_limit);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cell_peer_id: %s\n", cell_peer_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_public_path: %s\n", db_public_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_log_path: %s\n", db_log_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    sendStr(q, &crc);
    snprintf(q, sizeof q, "PID: %d\n", proc_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "prog_list length: %d\n", prog_list.length);
    sendStr(q, &crc);
    sendStr("+--------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                    Program                                     |\n", &crc);
    sendStr("+-----------+--------------------------------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    id     |           description          |check_int_s|cope_dur_s |phone_group|\n", &crc);
    sendStr("+-----------+--------------------------------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_DF
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%32.32s|%11ld|%11ld|%11d|\n",
            curr->id,
            curr->description,
            curr->check_interval.tv_sec,
            curr->cope_duration.tv_sec,
            curr->phone_number_group_id
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+--------------------------------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-----------------------------------------------+\n", &crc);
    sendStr("|                   Program runtime             |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|    id     |   state   |check_rst_s|cope_rst_s |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    PROG_LIST_LOOP_ST
            char *state = getStateStr(curr->state);
    struct timespec tm1 = getTimeRestCheck(curr);
    struct timespec tm2 = getTimeRestCope(curr);
    snprintf(q, sizeof q, "|%11d|%11.11s|%11ld|%11ld|\n",
            curr->id,
            state,
            tm1.tv_sec,
            tm2.tv_sec
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);

    sendStr("+-------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                       Peer                                          |\n", &crc);
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);
    sendStr("|               id               |   link    | sock_port |      addr      |     fd    |\n", &crc);
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);
    for (i = 0; i < peer_list.length; i++) {
        snprintf(q, sizeof q, "|%32.32s|%11p|%11u|%16u|%11d|\n",
                peer_list.item[i].id,
                &peer_list.item[i],
                peer_list.item[i].addr.sin_port,
                peer_list.item[i].addr.sin_addr.s_addr,
                *peer_list.item[i].fd
                );
        sendStr(q, &crc);
    }
    sendStr("+--------------------------------+-----------+-----------+----------------+-----------+\n", &crc);

    sendFooter(crc);
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tload prog into RAM and start its execution; program id expected if '.' quantifier is used\n", ACP_CMD_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM; program id expected if '.' quantifier is used\n", ACP_CMD_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM, after that load it; program id expected if '.' quantifier is used\n", ACP_CMD_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog runtime data in format:  progId_state_stateEM_output_timeRestSecToEMSwap; program id expected if '.' quantifier is used\n", ACP_CMD_ALP_PROG_GET_DATA_RUNTIME);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog initial data in format;  progId_setPoint_mode_ONFdelta_PIDheaterKp_PIDheaterKi_PIDheaterKd_PIDcoolerKp_PIDcoolerKi_PIDcoolerKd; program id expected if '.' quantifier is used\n", ACP_CMD_ALP_PROG_GET_DATA_INIT);
    sendStr(q, &crc);
    sendFooter(crc);
}