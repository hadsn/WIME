#include "qim.h"
#include <QX11Info>
#if QT_VERSION >= 0x050000
#include <QGuiApplication>
#endif
#include <QWidget>
#include <QTextCharFormat>
#include <X11/Xlib.h>
#undef KeyPress
#undef KeyRelease
#include "so/xres.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/cmdlineopt.h"
//#include <iostream>
//using namespace std;

const char IdName[] = "wime";
const char LangCode[] = "ja";
static ToggleKey* ToggleKeys;
ATImeCol ImeColor[ATIMECOMPCOL_ITEMMAX];

void QWime::create_wime_context()
{
    ServerLevel = RestartServerCount;
    if((WimeCxn = CannaCreateContext()) != -1){
	WimeShowToolbar(WimeCxn,true,false);
	WimeShowCandWin(WimeCxn,true);
	WimeSetFocus(WimeCxn,true);
    }
}

void QWime::replace_context()
{
    if(ServerLevel!=RestartServerCount || WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = WimeCxn;
	create_wime_context();
	update();
	DEBUGLOG(CH_QT,"replace wime context %d --> %d\n",old,WimeCxn);
    }
}

QWime::QWime(QObject* parent_qt4):
#if QT_VERSION < 0x050000
    InputContextBase(parent_qt4),
#endif				  
    FocusObj(nullptr)
{
    create_wime_context();
    DEBUGLOG(CH_QT,"parent=%p cxn=%d\n",parent_qt4,WimeCxn);
}

QWime::~QWime()
{
    DEBUGLOG(CH_QT,"cxn=%d\n",WimeCxn);
    WimeShowToolbar(WimeCxn,false,false);
    CannaCloseContext(WimeCxn);
}

#if QT_VERSION <= 0x050000
QString QWime::identifierName()
{
    DEBUGLOG(CH_QT,"returned name=%s\n",IdName);
    return IdName;
}

bool QWime::isComposing() const
{
    DEBUGLOG(CH_QT,"stub:return false\n");
    return false;
}

QString QWime::language()
{
    DEBUGLOG(CH_QT,"returned name=%s\n",LangCode);
    return LangCode;
}
#endif

void QWime::reset()
{
    INFOLOG(CH_QT,"stub\n");
}

void QWime::SendEvToFocusObj(QInputMethodEvent* ev)
{
#if QT_VERSION >= 0x050000
    QGuiApplication::sendEvent(FocusObject(),ev);
#else
    sendEvent(*ev);
#endif
}

QBrush mk_brush(uint32_t colorref)
{
    return QBrush(QColor((GETR(colorref)<<16)|(GETG(colorref)<<8)|GETB(colorref)));
}

void set_color(QTextCharFormat* tf,int index)
{
    tf->setForeground(mk_brush(ImeColor[index].Text));
    tf->setBackground(mk_brush(ImeColor[index].Back));
    tf->setFontUnderline(ImeColor[index].UnderLine);
}

extern "C" void qim_preedit(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    auto self = static_cast<QWime*>(arg);
    QList<QInputMethodEvent::Attribute> at;
    QTextCharFormat tf;

    set_color(&tf,ATCOLINDEX_INPUT);
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,0,si->Length,QVariant(tf)));
    
    QInputMethodEvent ev(QString::fromUtf8(u8),at);
    self->SendEvToFocusObj(&ev);
}

extern "C" void qim_commit(const char* u8,const char* composition,const WimeCompStrInfo* si,void* arg)
{
    QInputMethodEvent ev;
    ev.setCommitString(QString::fromUtf8(u8));
    (static_cast<QWime*>(arg))->SendEvToFocusObj(&ev);
    if(composition != NULL)
	qim_preedit(composition,si,arg);
}

extern "C" void qim_convert(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    QTextCharFormat ul,rv;
    QList<QInputMethodEvent::Attribute> at;
    auto self = static_cast<QWime*>(arg);

    set_color(&rv,ATCOLINDEX_TARGETCONVERT);    //注目文節
    set_color(&ul,ATCOLINDEX_CONVERTED);    //その他の文節

    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,0,si->Length,QVariant(ul)));
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,si->TargetClause,si->TargetClLen,QVariant(rv)));
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor,si->TargetClause,1,QVariant(0)));

    QInputMethodEvent ev(QString::fromUtf8(u8),at);
    self->SendEvToFocusObj(&ev);
}

bool QWime::filterEvent(const QEvent* ev)
{
    if(ev->type() != QEvent::KeyPress)
	return false;

    auto kev = dynamic_cast<const QKeyEvent*>(ev);
    
    DEBUGLOG(CH_QT,"keypress mod %x sc %x vk %x key %x (mod %x key %x)\n",kev->nativeModifiers(),kev->nativeScanCode(),kev->nativeVirtualKey(),kev->key(),ToggleKeys->Mod,ToggleKeys->Key);

    replace_context();
    bool st=WimeFilterKey(WimeCxn,ToggleKeys,QX11Info::display(),kev->nativeScanCode(),kev->nativeVirtualKey(),kev->nativeModifiers(),this);
    DEBUGLOG(CH_QT,"return code: %d\n",st);
    return st;
}

//wのトップレベルウィジェットに対する相対座標を求める
static QPoint rel_pos(const QWidget* w)
{
    QPoint pos(0,0);
    do{
	pos += w->pos();
    }while(w = w->parentWidget(),w!=NULL&&w->parentWidget()!=NULL);
    return pos;
}

#if QT_VERSION >= 0x050000
void QWime::setFocusObject(QObject* object)
{
    if(FocusObj != object){
	FocusObj = object;
    }
}

bool QWime::isValid() const
{
    return WimeCxn!=-1;
}

void QWime::update(Qt::InputMethodQueries q)
{
    DEBUGLOG(CH_QT,"query %x\n",(unsigned)q);
    update();
}
#endif

QObject* QWime::FocusObject()
{
#if QT_VERSION >= 0x050000
    return FocusObj;
#else
    return focusWidget();
#endif
}

void QWime::update()
{
    QWidget* w = qobject_cast<QWidget*>(FocusObject());
    if(w){
	//候補ウィンドウをカーソルの下に移動させる

	/*トップレベルウィジェットの位置を求める。
	  QWiget::geometry()などではウィンドウマネージャによる装飾枠が含まれないので、
	  XTranslateCoordinates()でルートウィンドウに対する相対位置を得る。*/
	Window dum_w;
	int topx=0,topy=0;
	auto p = w->nativeParentWidget();
	if(!p)
	    p = w;
	XTranslateCoordinates(QX11Info::display(),p->effectiveWinId(),QX11Info::appRootWindow(),0,0,&topx,&topy,&dum_w);

	//(トップレベルウィジェットの位置+wの位置)がルートウィンドウ上での位置になる。
	QPoint pos;
	if(p!=w)
	    pos = rel_pos(w);
	WimeMoveShadowWin(WimeCxn,topx+pos.x(),topy+pos.y(),w->width(),w->height());

	//候補ウィンドウは編集している行の下にしたいので、カーソルの場所と高さを得る。
	//(行のすぐ下に出て見づらいので4ポイント下げる。この数値はいいかげん)
	auto rect = w->inputMethodQuery(/*Qt::ImCursorRectangle*/Qt::ImMicroFocus).toRect();
	WimeSetCandWin(WimeCxn,WIME_POS_POINT,rect.x(),rect.y()+rect.height()+4);
    }
}

//////////////////////////////////////////////////////////////////

static void catch_restart_signal(void)
{
    ++RestartServerCount;
    DEBUGLOG(CH_QT,"count %d\n",RestartServerCount);
    WimeGetColor(0,ImeColor);
}

QWimePlugin::QWimePlugin(QObject* parent):PluginBase(parent)
{
    WimeInitialize(ParseEnv(CH_QT|CH_GLOBAL),'q');
    InitDatabase(QX11Info::display(),"qim");
    ToggleKeys = GetConvKeyFromResource(QX11Info::display());
    WimePreedit = qim_preedit;
    WimeConvert = qim_convert;
    WimeCommit = qim_commit;
    WimeRestartSignal(catch_restart_signal);
    WimeGetColor(0,ImeColor);
    DEBUGLOG(CH_QT,"Qt version " QT_VERSION_STR ", parent=%p\n",parent);
}

QWimePlugin::~QWimePlugin()
{
    DEBUGLOG(CH_QT,"\n");
    WimeFinalize();
    free(ToggleKeys);
}

InputContextBase* QWimePlugin::create(const QString& key
#if QT_VERSION >= 0x050000
				      ,const QStringList& paramList UNUSED
#endif
    )
{
    DEBUGLOG(CH_QT,"key=%s\n",key.toStdString().c_str());
    return key.toLower()==IdName ? new QWime : nullptr;
}
	
#if QT_VERSION < 0x050000

QString QWimePlugin::description(const QString& key)
{
    DEBUGLOG(CH_QT,"key=%s\n",key.toStdString().c_str());
    return "wime";
}

QString QWimePlugin::displayName(const QString& key)
{
    DEBUGLOG(CH_QT,"key=%s\n",key.toStdString().c_str());
    return "wime";
}

QStringList QWimePlugin::keys() const
{
    return {IdName};
}

QStringList QWimePlugin::languages(const QString& key)
{
    DEBUGLOG(CH_QT,"key=%s\n",key.toStdString().c_str());
    return {LangCode};
}

Q_EXPORT_PLUGIN2(wimeqim,QWimePlugin)
#endif

//(C) 2020 thomas
