#include "engine.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "inf.h"
#include <stdbool.h>
#include <getopt.h>

static IBusBus* bus = NULL;
static IBusFactory* factory = NULL;
Display* Disp;
int Flags;
int SocketNum;

static void ibus_disconnected_cb(IBusBus* bus,gpointer user_data)
{
    ibus_quit ();
}


#define STR2(s) #s
#define STR(s) STR2(s)

static void init(bool exec_by_ibus)
{
    IBusComponent* component;

    ibus_init();

    bus = ibus_bus_new();
    g_object_ref_sink(bus);
    g_signal_connect(bus,"disconnected",G_CALLBACK(ibus_disconnected_cb),NULL);
	
    factory = ibus_factory_new(ibus_bus_get_connection(bus));
    g_object_ref_sink(factory);
    ibus_factory_add_engine(factory,"wime",IBUS_TYPE_WIME_ENGINE);

    component = ibus_component_new(STR(COMPNAME),
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
	ibus_bus_request_name(bus,STR(COMPNAME),0);
    else
	ibus_bus_register_component(bus,component);
}

void usage()
{
    printf(
"-d,--daemonize	daemonize ibus\n"
"-i,--ibus	executed by ibus.\n"
"-C		ibusの候補ウィンドウを使う\n"
"-p <num>	ソケットに追加する番号\n"
"-h,--help	この表示\n");
}

int main(int ac,char* av[])
{
    int c;
    struct option longopt[]={
	{"help",no_argument,NULL,'h'},
	{"daemonize",no_argument,NULL,'d'},
	{"ibus",no_argument,NULL,'i'},
	{NULL,0,NULL,0}};
    bool exec_by_ibus=false;

    while((c=getopt_long(ac,av,"dihCp:",longopt,NULL))!=-1){
	switch(c){
	case 'C':
	    Flags |= USE_IBUS_CANDIDATE_WINDOW;
	    break;
	case 'h':
	    usage();
	    exit(0);
	case 'd':
	    switch(fork()){
	    case -1:
		perror(NULL);
		exit(1);
	    case 0:
		exit(0);
	    }
	    break;
	case 'i':
	    exec_by_ibus=true;
	    break;
	case 'p':
	    SocketNum = atoi(optarg);
	    break;
	}
    }
    init(exec_by_ibus);
    Disp = XOpenDisplay(NULL);
    ibus_main();
    return 0;
}

//(C) 2012 thomas
