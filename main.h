
#ifndef CHP_H
#define CHP_H


#include "lib/util.h"
#include "lib/crc.h"
#include "lib/app.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/configl.h"
#include "lib/logl.h"
#include "lib/dbl.h"
#include "lib/tsv.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/prog.h"
#include "lib/acp/mobile.h"



#define APP_NAME chp
#define APP_NAME_STR TOSTRING(APP_NAME)


#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif

#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define WAIT_RESP_TIMEOUT 3
#define GOOD_COUNT 7

#define PROG_LIST_LOOP_ST {Prog *item = prog_list.top; while (item != NULL) {
#define PROG_LIST_LOOP_SP item = item->next; } item = prog_list.top;}

enum {
    INIT,
    RUN,
    STOP,
    RESET,
    OFF,
    FAILURE,
    DISABLE,
    WBAD,
    WCOPE,
    WGOOD
} StateAPP;

struct prog_st {
    int id;
    Peer peer;
    EM em;
    struct timespec check_interval;
    struct timespec cope_duration;
    int g_count;
    char state;
    Ton_ts tmr_check;
    Ton_ts tmr_cope;
    
    struct timespec cycle_duration;
    int sock_fd;
    Mutex mutex;
    pthread_t thread;
    struct prog_st *next;
};
typedef struct prog_st Prog;

DEC_LLIST(Prog)

#define PHONE_SIZE 12
typedef struct {
    int id;
    char value[PHONE_SIZE];
} Phone;
DEC_LIST(Phone)

typedef struct {
    ProgList *prog_list;
    PeerList *peer_list;
    Prog * prog;
    sqlite3 *db_data;
} ProgData;

extern int initData();

extern void initApp();

extern void serverRun(int *state, int init_state);

extern void progControl(Prog *item);

extern void *threadFunction(void *arg);

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);
#endif 

