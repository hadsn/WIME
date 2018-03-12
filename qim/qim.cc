#include "qim.h"
#include <QX11Info>
#include <QEvent>
#include <QTextFormat>
#include <X11/Xlib.h>
#undef KeyPress
#undef KeyRelease
#include "so/xres.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "so/winkey.h"
#include "lib/wimeconn.h"
#include "lib/log.h"

const char IdName[] = "wime";
const char LangCode[] = "ja";
static ToggleKey* ToggleKeys;

void Qim::create_wime_context()
{
    ServerLevel = RestartServerCount;
    WimeCxn = CannaCreateContext();
    WimeShowToolbar(WimeCxn,true,false);
    WimeShowCandidateWindow(WimeCxn,true);
    WimeSetFocus(WimeCxn,true);
}

void Qim::replace_context()
{
    if(ServerLevel!=RestartServerCount || WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = WimeCxn;
	create_wime_context();
	update();
	LOG(CH_QT,LOG_DEBUG,MESG("replace wime context %d --> %d\n",old,WimeCxn));
    }
}

Qim::Qim(QObject* parent):QInputContext(parent),Enabled(false)
{
    create_wime_context();
    LOG(CH_QT,LOG_DEBUG,MESG("parent=%p cxn=%d\n",parent,WimeCxn));
}

Qim::~Qim()
{
    LOG(CH_QT,LOG_DEBUG,MESG("cxn=%d\n",WimeCxn));
    WimeShowToolbar(WimeCxn,false,false);
    CannaCloseContext(WimeCxn);
}

QString Qim::identifierName()
{
    LOG(CH_QT,LOG_DEBUG,MESG("returned name=%s\n",IdName));
    return IdName;
}

bool Qim::isComposing() const
{
    LOG(CH_QT,LOG_DEBUG,MESG("stub:return false\n"));
    return false;
}

QString Qim::language()
{
    LOG(CH_QT,LOG_DEBUG,MESG("returned name=%s\n",LangCode));
    return LangCode;
}

void Qim::reset()
{
    LOG(CH_GLOBAL|CH_QT,LOG_MESSAGE,MESG("stub\n"));
}

void qim_preedit(const char* ej,const WimeCompStrInfo* si,void* arg)
{
    QList<QInputMethodEvent::Attribute> at;
    QTextCharFormat tf;
    char* u8 = EjToU8(NULL,ej);

    tf.setFontUnderline(true);
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,0,si->Length,QVariant(tf)));
    
    QInputMethodEvent e(QString::fromUtf8(u8),at);
    (static_cast<Qim*>(arg))->sendEvent(e);
    free(u8);
}

void qim_commit(const char* ej,void* arg)
{
    QInputMethodEvent e;
    char* u8 = EjToU8(NULL,ej);

    e.setCommitString(QString::fromUtf8(u8));
    (static_cast<Qim*>(arg))->sendEvent(e);
    free(u8);
}

void qim_convert(const char* ej,const WimeCompStrInfo* si,void* arg)
{
    QTextCharFormat ul,rv;
    QList<QInputMethodEvent::Attribute> at;
    Qim* self = static_cast<Qim*>(arg);
    char* u8 = EjToU8(NULL,ej);

    //注目文節は反転させる。 standardFormat()を使うのか？
    QRgb c = self->focusWidget()->palette().text().color().rgb();
    rv.setForeground(QBrush(QColor(~c)));
    rv.setBackground(QBrush(QColor(c)));
    //その他の文節
    ul.setFontUnderline(true);

    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,0,si->Length,QVariant(ul)));
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat,si->TargetClause,si->TargetClLen,QVariant(rv)));
    at.append(QInputMethodEvent::Attribute(QInputMethodEvent::Cursor,si->TargetClause,1,QVariant(0)));

    QInputMethodEvent e(QString::fromUtf8(u8),at);
    self->sendEvent(e);
    free(u8);
}

bool Qim::filterEvent(const QEvent* ev)
{
    if(ev->type() != QEvent::KeyPress)
	return false;

    auto kev = dynamic_cast<const QKeyEvent*>(ev);
    KeySym sym = kev->nativeVirtualKey();
    if((kev->nativeModifiers() & MODESWITCHMASK) == 0){
	sym = KeycodeToKeysym(QX11Info::display(),kev->nativeScanCode(),kev->nativeModifiers(),0);//XKEYCODETOKEYSYM(QX11Info::display(),kev->nativeScanCode(),0);
    }
    
    LOG(CH_QT,LOG_DEBUG,MESG("keypress mod %x sc %x vk %x key %x (mod %x key %x)\n",kev->nativeModifiers(),kev->nativeScanCode(),kev->nativeVirtualKey(),kev->key(),ToggleKeys->Mod,ToggleKeys->Key));

    replace_context();
    bool st=WimeFilterKey(WimeCxn,ToggleKeys,sym,kev->nativeModifiers(),this);
    LOG(CH_QT,LOG_DEBUG,MESG("return code: %d\n",st));
    return st;
}

//wのトップレベルウィジェットに対する相対座標を求める
static QPoint global_pos(const QWidget* w)
{
    QPoint pos(0,0);
    do{
	pos += w->pos();
    }while(w = w->parentWidget(),w!=NULL&&w->parentWidget()!=NULL);
    return pos;
}

void Qim::update()
{
    QWidget* w;
    if((w = focusWidget()) != NULL){
	//候補ウィンドウをカーソルの下に移動させる

	/*トップレベルウィジェットの位置を求める。
	  QWiget::geometry()などではウィンドウマネージャによる装飾枠が含まれないので、
	  XTranslateCoordinates()でルートウィンドウに対する相対位置を得る。*/
	Window dum_w;
	int topx,topy;
	auto p=w->nativeParentWidget();
	XTranslateCoordinates(p->x11Info().display(),p->effectiveWinId(),p->x11Info().appRootWindow(),0,0,&topx,&topy,&dum_w);

	//(トップレベルウィジェットの位置+wの位置)がルートウィンドウ上での位置になる。
	auto pos = global_pos(w);
	WimeMoveShadowWin(WimeCxn,topx+pos.x(),topy+pos.y(),w->width(),w->height());

	//候補ウィンドウは編集している行の下にしたいので、カーソルの場所と高さを得る。
	//(行のすぐ下に出て見づらいので4ポイント下げる。この数値はいいかげん)
	auto rect = w->inputMethodQuery(Qt::ImMicroFocus).toRect();
	WimeSetCandWin(WimeCxn,WIME_POS_POINT,rect.x(),rect.y()+rect.height()+4);
    }
}


//////////////////////////////////////////////////////////////////

WimeQimPlugin::WimeQimPlugin(QObject* parent):QInputContextPlugin(parent)
{
    ParseChannelEnv(CH_QT|CH_GLOBAL);
    WimeInitialize(0,'q');
    InitDatabase(NULL,"qim");
    ToggleKeys = GetConvKeyFromResource(QX11Info::display());
    WimePreedit = qim_preedit;
    WimeConvert = qim_convert;
    WimeCommit = qim_commit;
    WimeRestartSignal(NULL,0);
    LOG(CH_QT,LOG_DEBUG,MESG("parent=%p\n",parent));
}

WimeQimPlugin::~WimeQimPlugin()
{
    LOG(CH_QT,LOG_DEBUG,MESG("\n"));
    WimeFinalize();
}

QInputContext* WimeQimPlugin::create(const QString& key)
{
    QInputContext* c = NULL;
    if(key.toLower() == IdName)
	c = new Qim;
    LOG(CH_QT,LOG_DEBUG,MESG("key=%s object=%p\n",key.toAscii().data(),c));
    return c;
}

QString WimeQimPlugin::description(const QString& key)
{
    LOG(CH_QT,LOG_DEBUG,MESG("key=%s\n",key.toAscii().data()));
    return "wime";
}

QString WimeQimPlugin::displayName(const QString& key)
{
    LOG(CH_QT,LOG_DEBUG,MESG("key=%s\n",key.toAscii().data()));
    return "wime";
}

QStringList WimeQimPlugin::keys() const
{
    return QStringList()<<IdName;
}

QStringList WimeQimPlugin::languages(const QString& key)
{
    return QStringList()<<LangCode;
}

Q_EXPORT_PLUGIN2(wimeqim,WimeQimPlugin)

//(C) 2011 thomas
