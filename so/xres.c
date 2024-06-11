// -*- coding:euc-jp -*-
#define _GNU_SOURCE /*asprintf*/
#include <X11/Xresource.h>
#include <X11/XKBlib.h> /*XkbKeycodeToKeysym*/
#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "xres.h"
#include "lib/log.h"
#include "lib/array.h"

static const char AppBase[] = "wime";
static const char AppClass[] = "Wime";
static char* AppName; //"."がついている

const char XResConvKey[] = "imeToggleKey";
const char XResOnKey[] = "imeOnKey";
const char XResOffKey[] = "imeOffKey";
const char XResDefFont[] = "defaultCompositionFont";
const char XResDisableSty[] = "disableInputStyle";

/*
  disp==NULLの場合XrmSetDatabaseをしない
*/
void InitDatabase(Display* disp,const char* postfix)
{
    if(disp != NULL){
	char* mgr = XResourceManagerString(disp);
	if(mgr != NULL){
	    XrmDatabase db = XrmGetStringDatabase(mgr);
	    XrmSetDatabase(disp,db);
	}
    }

    asprintf(&AppName,"%s%s.",AppBase,postfix);
}

const char* GetResource(Display* disp,const char* res)
{
    int ressize = strlen(res)+1;
    char *type,name[strlen(AppName)+ressize+1],cls[sizeof(AppClass)+ressize+1],rescls[ressize+1];
    XrmValue rv;
    XrmDatabase db;

    if((db = XrmGetDatabase(disp)) == NULL){
	return NULL;
    }

    //リソース名のクラスとしてとりあえず先頭を大文字にしておく。
    //"defaultCompositionFont"のクラスは"Font"とかにするか？
    strcpy(rescls,res);
    rescls[0] = toupper(rescls[0]);

    sprintf(name,"%s%s",AppName,res);
    sprintf(cls,"%s.%s",AppClass,rescls);
    return XrmGetResource(db,name,cls,&type,&rv) ? rv.addr : NULL;
}
    
/*
  変換開始キーをリソースから取得する
  lstはToggleKeyの配列
*/
void get_conv_key(Display* disp,const char* resname,ImeStateKeyType type,Array* lst)
{
    const char* cres = GetResource(disp,resname);
    if(cres  == NULL)
	return;

    const char* keysep = " \t\n";
    char* res_orig = strdup(cres);
    char* res = strtok(res_orig,keysep);
    do{
	ToggleKey kl = {.Type=type, .Mod=0};
	char* sep = strchr(res,'-');
	if(sep != NULL){
	    //'-'があればそれより前を修飾キーとする
	    for(; res!=sep; ++res){
		switch(*res){
		case 'S': //shift
		    kl.Mod |= ShiftMask;
		    break;
		case 'C': //ctrl
		    kl.Mod |= ControlMask;
		    break;
		case 'M': //alt
		case 'A': //alt
		case '1':
		    kl.Mod |= Mod1Mask;
		    break;
		case '2':
		    kl.Mod |= Mod2Mask;
		    break;
		case '3':
		    kl.Mod |= Mod3Mask;
		    break;
		case 'W': //super(mod4)
		case 's': //super(mod4)
		case '4':
		    kl.Mod |= Mod4Mask;
		    break;
		case '5':
		    kl.Mod |= Mod5Mask;
		    break;
		default:
		    printf("unknown state mask %c\n",*res);
		}
	    }
	    ++res; // '-'の次の位置へ
	}
	if(res[1]==0 || isspace(res[1]))
	    kl.Key = res[0]; //１文字
	else
	    kl.Key = XStringToKeysym(res); //２文字以上＝キー名
	if(kl.Key == NoSymbol)
	    kl.Key = XK_VoidSymbol;
	ArAdd1(lst,&kl);
	DEBUGLOG(CH_GLOBAL,"type %d add %x/%x\n",type,kl.Key,kl.Mod);
    }while((res = strtok(NULL,keysep)) != NULL);
    free(res_orig);
}

/*
  変換開始キーをリソースから取得する
  戻り値はfreeすること
  リストの最後は{0,0,0}
*/
ToggleKey* GetConvKeyFromResource(Display* disp)
{
    Array lst;
    ArNew(&lst,sizeof(ToggleKey),NULL);
    get_conv_key(disp,XResConvKey,IMESTATUS_TOGGLE,&lst);
    get_conv_key(disp,XResOnKey,IMESTATUS_ON,&lst);
    get_conv_key(disp,XResOffKey,IMESTATUS_OFF,&lst);
    if(ArUsing(&lst))
	ArAdd1(&lst,&(ToggleKey){0,0,0});
    return ArAdr(&lst);
}

ImeStateKeyType IsToggleKey(const ToggleKey* keylist,unsigned key,unsigned mod)
{
    ImeStateKeyType st = IMESTATUS_NO_TOGGLE;
    if(keylist != NULL){
	mod &= 0xffff; //SUPER_MASK,HYPER_MASK,META_MASKなどは無視する
	mod &= ~(Mod2Mask|MODESWITCHMASK); //numlock,modeswitchも無視する。
	for(; keylist->Key!=0; ++keylist){
	    DEBUGLOG(CH_GLOBAL,"key %x/%x, list %x/%x\n",key,mod,keylist->Key,keylist->Mod);
	    if(keylist->Key==key && keylist->Mod==mod){
		st = keylist->Type;
		break;
	    }
	}
    }
    return st;
}

/*
  変換ウィンドウのデフォルトフォントをxlfdで返す
*/
char* GetCompFont(Display* disp)
{
    const char *res,fmt[]="-%s-%s-*-*-*--%d-*-*-*-*-*-jisx0208-*";
    char* fnt=NULL;

    if((res = GetResource(disp,XResDefFont)) != NULL){
	int n=strlen(res),h;
	char fndy[n],fmly[n];
	if(sscanf(res,"%d,%s %s",&h,fndy,fmly) == 3){
	    char xlfd[sizeof(fmt)+n];
	    sprintf(xlfd,fmt,fndy,fmly,h);
	    fnt = strdup(xlfd);
	}else
	    printf("%s:bad font:%s\n",__func__,res);
    }
    return fnt;
}

/*
  keysym配列(xmodmapの定義式)の0番目か2番め(mode_switchがかかっているとき)を返す。
  shiftlevelは0か1。-1のときはstateのshift-maskの状態を使用する。
 */
KeySym KeycodeToKeysym(Display* disp,KeyCode kc,unsigned state,int shiftlevel)
{
    unsigned grp = (state & MODESWITCHMASK) ? 1:0;
    if(shiftlevel < 0)
	shiftlevel = (state & ShiftMask) ? 1:0;
    return XkbKeycodeToKeysym(disp,kc,grp,shiftlevel);
}

//(C) 2009 thomas
