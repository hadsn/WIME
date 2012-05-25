#include <X11/Xresource.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "xres.h"

static const char AppBase[] = "wime";
static const char AppClass[] = "Wime.";
static const char *AppName; //"."がついている

const char XResConvKey[] = "imeToggleKey";
const char XResDefFont[] = "defaultCompositionFont";
const char XResDisableSty[] = "disableInputStyle";

/*
  disp==NULLの場合XrmSetDatabaseをしない
*/
void InitDatabase(Display* disp,const char* postfix)
{
    if(disp != NULL){
	XrmDatabase db = XrmGetStringDatabase(XResourceManagerString(disp));
	XrmSetDatabase(disp,db);
    }

    char buf[sizeof(AppBase)+strlen(postfix)+1];
    sprintf(buf,"%s%s.",AppBase,postfix);
    AppName = strdup(buf);
}

const char* GetResource(Display* disp,const char* res)
{
    int ressize = strlen(res)+1;
    char *type,name[strlen(AppName)+ressize],cls[sizeof(AppClass)+ressize],rescls[ressize];
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
    sprintf(cls,"%s%s",AppClass,rescls);
    return XrmGetResource(db,name,cls,&type,&rv) ? rv.addr : NULL;
}
    
int count_char(const char* s,char c)
{
    int n;
    for(n=0; (s = strchr(s,c))!=NULL; ++s,++n)
	;
    return n;
}
	
/*
  変換開始キーをリソースから取得する
  戻り値はfreeすること
  リストの最後は{0,0}
*/
ToggleKey* GetConvKeyFromResource(Display* disp)
{
    const char *res;
    char *sep;
    ToggleKey *kl,*kl0;
    int sz;

    if((res = GetResource(disp,XResConvKey)) == NULL)
	return NULL;

    sz = (count_char(res,'\n')+2)*sizeof(*kl);
    kl = kl0 = memset(malloc(sz),0,sz);
    do{
	while(isspace(*res))
	    ++res;
	if((sep = strchr(res,'-')) != NULL){
	    //'-'があればそれより前を修飾キーとする
	    for(; res!=sep; ++res){
		switch(*res){
		case 'S': //shift
		    kl->Mod |= ShiftMask;
		    break;
		case 'C': //ctrl
		    kl->Mod |= ControlMask;
		    break;
		case 'M': //alt
		case 'A': //alt
		case '1':
		    kl->Mod |= Mod1Mask;
		    break;
		case '2':
		    kl->Mod |= Mod2Mask;
		    break;
		case '3':
		    kl->Mod |= Mod3Mask;
		    break;
		case 'W': //super
		case '4':
		    kl->Mod |= Mod4Mask;
		    break;
		case '5':
		    kl->Mod |= Mod5Mask;
		    break;
		default:
		    printf("unknown state mask %c\n",*res);
		}
	    }
	    ++res; // '-'の次の位置へ
	}
	if(res[1]==0 || isspace(res[1]))
	    kl->Key = res[0]; //１文字
	else
	    kl->Key = XStringToKeysym(res); //２文字以上＝キー名
	++kl;
    }while((res = strchr(res,'\n')) != NULL);
    return kl0;
}

bool IsToggleKey(const ToggleKey* keylist,unsigned key,unsigned mod)
{
    bool st=false;
    mod &= 0xffff; //SUPER_MASK,HYPER_MASK,META_MASKなどは無視する
    for(; keylist->Key!=0; ++keylist){
	if(keylist->Key==key && keylist->Mod==mod){
	    st = true;
	    break;
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
	char fndy[n],fmly[n],xlfd[sizeof(fmt)+n];
	if(sscanf(res,"%d,%s %s",&h,fndy,fmly) == 3){
	    sprintf(xlfd,fmt,fndy,fmly,h);
	    fnt = strdup(xlfd);
	}else
	    printf("%s:bad font:%s\n",__func__,res);
    }
    return fnt;
}
