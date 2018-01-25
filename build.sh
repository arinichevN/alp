#!/bin/bash

APP=alp
APP_DBG=`printf "%s_dbg" "$APP"`
INST_DIR=/usr/sbin
CONF_DIR=/etc/controller
CONF_DIR_APP=$CONF_DIR/$APP

#DEBUG_PARAM="-Wall -pedantic"
DEBUG_PARAM="-Wall"
MODE_DEBUG=-DMODE_DEBUG
MODE_FULL=-DMODE_FULL

PLATFORM=-DPLATFORM_ANY

NONE=-DNONEANDNOTHING


function move_bin {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp $APP $INST_DIR/$APP && \
	chmod a+x $INST_DIR/$APP && \
	chmod og-w $INST_DIR/$APP && \
	echo "Your $APP executable file: $INST_DIR/$APP";
}

function move_bin_dbg {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp $APP_DBG $INST_DIR/$APP_DBG && \
	chmod a+x $INST_DIR/$APP_DBG && \
	chmod og-w $INST_DIR/$APP_DBG && \
	echo "Your $APP executable file for debugging: $INST_DIR/$APP_DBG";
}

function move_conf {
	([ -d $CONF_DIR ] || mkdir $CONF_DIR) && \
	([ -d $CONF_DIR_APP ] || mkdir $CONF_DIR_APP) && \
	cp  config.tsv $CONF_DIR_APP && \
	cp  db/data.db $CONF_DIR_APP && \
	chmod -R a+w $CONF_DIR_APP
	echo "Your $APP configuration files are here: $CONF_DIR_APP";
}

#your application will run on OS startup
function conf_autostart {
	cp -v init.sh /etc/init.d/$APP && \
	chmod 755 /etc/init.d/$APP && \
	update-rc.d -f $APP remove && \
	update-rc.d $APP defaults 30 && \
	echo "Autostart configured";
}

function build_lib {
	gcc $1 $PLATFORM -c app.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c crc.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c timef.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c udp.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c util.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c dbl.c -D_REENTRANT -DSQLITE_THREADSAFE=2 -DSQLITE_OMIT_LOAD_EXTENSION $DEBUG_PARAM -lsqlite3 -lpthread && \
	gcc $1 $PLATFORM -c configl.c -D_REENTRANT -DSQLITE_THREADSAFE=2 -DSQLITE_OMIT_LOAD_EXTENSION $DEBUG_PARAM -lsqlite3 -lpthread && \
	gcc $1 $PLATFORM -c logl.c  -D_REENTRANT -DSQLITE_THREADSAFE=2 -DSQLITE_OMIT_LOAD_EXTENSION $DEBUG_PARAM -lsqlite3 -lpthread && \
	cd acp && \
	gcc $PLATFORM -c main.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	gcc $1 $PLATFORM -c mobile.c -D_REENTRANT $DEBUG_PARAM -lpthread && \
	cd ../ && \
	echo "library: making archive..." && \
	rm -f libpac.a
	ar -crv libpac.a app.o crc.o timef.o udp.o util.o dbl.o configl.o logl.o acp/main.o acp/mobile.o && echo "library: done"
	rm -f *.o acp/*.o
}

#    1         2
#debug_mode bin_name
function build {
	cd lib && \
	build_lib $1 && \
	cd ../ 
	gcc -D_REENTRANT -DSQLITE_THREADSAFE=2 -DSQLITE_OMIT_LOAD_EXTENSION $1 $3 $PLATFORM $INI_MODE main.c -o $2 $DEBUG_PARAM -lpthread -lsqlite3 -L./lib -lpac && echo "Application successfully compiled. Launch command: sudo ./"$2
}

function full {
	DEBUG_PARAM=$NONE
	build $NONE $APP $MODE_FULL && \
	build $MODE_DEBUG $APP_DBG $MODE_FULL && \
	move_bin && move_bin_dbg && move_conf && conf_autostart
}
function full_nc {
	DEBUG_PARAM=$NONE
	build $NONE $APP $MODE_FULL && \
	build $MODE_DEBUG $APP_DBG $MODE_FULL  && \
	move_bin && move_bin_dbg
}
function part_debug {
	build $MODE_DEBUG $APP_DBG $NONE
}
function uninstall_nc {
	pkill $APP --signal 9
	pkill $APP_DBG --signal 9
	rm -f $INST_DIR/$APP
	rm -f $INST_DIR/$APP_DBG
}
function uninstall {
	uninstall_nc
	update-rc.d -f $APP remove
	rm -rf $CONF_DIR_APP
}

f=$1
${f}