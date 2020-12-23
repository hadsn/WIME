#include "engine.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include "inf.h"
#include <stdbool.h>
#include "lib/log.h"
#include "lib/cmdlineopt.h"
#include "lib/printf.h"
#include "lib/ut.h"

static IBusBus* Bus = NULL;
static IBusFactory* Factory = NULL;
Display* Disp;
int Flags;
int SocketNum;

static void ibus_disconnected_cb(IBusBus* bus,gpointer user_data)
{
    ibus_quit();
}


#define STR2(s) #s
#define STR(s) STR2(s)

static void init(bool exec_by_ibus)
{
    ibus_init();

    Bus = ibus_bus_new();
    g_object_ref_sink(Bus);
    g_signal_connect(Bus,"disconnected",G_CALLBACK(ibus_disconnected_cb),NULL);
	
    Factory = ibus_factory_new(ibus_bus_get_connection(Bus));
    g_object_ref_sink(Factory);
    ibus_factory_add_engine(Factory,"wime",IBUS_TYPE_WIME_ENGINE);

    IBusComponent* component = ibus_component_new(STR(COMPNAME),
						  STR(COMPDSC),
						  STR(VERSION),
						  STR(LICENSE),
						  STR(AUT),
						  STR(HOMEPAGE),
						  ""/*STR(EXEC)*/,
						  STR(TEXTDOMAIN));
    ibus_component_add_engine(component,
			      ibus_engine_desc_new("wime",
						   STR(DSC),
						   STR(DSC),
						   STR(LANGUAGE),
						   STR(LICENSE),
						   STR(AUT),
						   STR(ICON),
						   STR(LAYOUT)));
    if(exec_by_ibus)
	ibus_bus_request_name(Bus,STR(COMPNAME),0);
    else
	ibus_bus_register_component(Bus,component);
}

bool set_candwin_flag(const char* arg,void* flags)
{
    *(int*)flags |= USE_IBUS_CANDIDATE_WINDOW;
    return true;
}

bool set_daemon_mode(const char* arg,void* unused)
{
    int pid = fork();
    if(pid == -1){
	perror(NULL);
	exit(1);
    }else if(pid != 0){
	exit(0);
    }
    return true;
}

bool set_exe_flag(const char* arg,void* to_bool)
{
    *(bool*)to_bool = true;
    return true;
}

bool cl_opt(int ac,char* av[])
{
    bool exe_flag=false;
    OptArg oa[]={
	{NULL,'C',no_argument,		set_candwin_flag,&Flags,"\tuse ibus candidate window",NULL},
	{"daemonize",'d',no_argument,	set_daemon_mode,NULL,"daemon mode",NULL},
	{"ibus",'i',no_argument,	set_exe_flag,&exe_flag,"executed by ibus",NULL},
    };
    SocketNum = CmdlineOpt(ac,av,oa,ITEMS(oa),NULL);
    return exe_flag;
}

int main(int ac,char* av[])
{
    CustomPrintf();
    bool exec_by_ibus = cl_opt(ac,av);
    if(SocketNum < 0)
	return 1;
    
    Disp = XOpenDisplay(NULL);
    InitDatabase(Disp,"wimeibus");
    init(exec_by_ibus);
    ibus_main();
    return 0;
}

//(C) 2012 thomas
