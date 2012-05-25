#include "qim.h"
#include <X11/Xlib.h>
#undef KeyPress
#include "so/xres.h"
#include "so/wimeapi.h"
#include "so/wimelog.h"
#include "lib/ut.h"
#include <QX11Info>
#include <QEvent>
#include <QTextFormat>
#include "so/winkey.h"


const char IdName[] = "wime";
const char LangCode[] = "ja";
static ToggleKey* ToggleKeys;


Qim::Qim(QObject* parent):QInputContext(parent),Enabled(false)
{
    WimeCxn = CannaCreateContext();
    WimeShowToolbar(WimeCxn,true,false);
    WimeSetFocus(WimeCxn,true);
    LOG("parent=%p cxn=%d\n",parent,WimeCxn);
}

Qim::~Qim()
{
    LOG("cxn=%d\n",WimeCxn);
    WimeShowToolbar(WimeCxn,false,false);
    CannaCloseContext(WimeCxn);
}

QString Qim::identifierName()
{
    LOG("returned name=%s\n",IdName);
    return IdName;
}

bool Qim::isComposing() const
{
    LOG("stub:return false\n");
    return false;
}

QString Qim::language()
{
    LOG("returned name=%s\n",LangCode);
    return LangCode;
}

void Qim::reset()
{
    LOG("stub\n");
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
    KeySym sym;

    if(ev->type() != QEvent::KeyPress)
	return false;

    const QKeyEvent* kev = dynamic_cast<const QKeyEvent*>(ev);
    sym = XKeycodeToKeysym(QX11Info::display(),kev->nativeScanCode(),0);

    LOG("keypress mod %x sc %x vk %x key %x (mod %x key %x)\n",kev->nativeModifiers(),kev->nativeScanCode(),kev->nativeVirtualKey(),kev->key(),ToggleKeys->Mod,ToggleKeys->Key);

    return WimeFilterKey(WimeCxn,ToggleKeys,sym,kev->nativeModifiers(),this);
}

//wのディスプレイ上の座標を求める
static QPoint global_pos(const QWidget* w)
{
    QPoint pos(w->geometry().x(),w->geometry().y());
    while((w = w->parentWidget()) != NULL){
	pos += QPoint(w->geometry().x(),w->geometry().y());
    }
    return pos;
}

void Qim::update()
{
    QWidget* w;
    if((w = focusWidget()) != NULL){
	//候補ウィンドウをカーソルの下に移動させる
	QVariant v = w->inputMethodQuery(Qt::ImMicroFocus);
	QRect rect = v.toRect();
	QPoint cs_pos = global_pos(focusWidget());
	cs_pos += QPoint(rect.x(),rect.y()+rect.height());
	WimeSetCandWin(WimeCxn,WIME_POS_POINT,cs_pos.x(),cs_pos.y());
    }
}


//////////////////////////////////////////////////////////////////

WimeQimPlugin::WimeQimPlugin(QObject* parent):QInputContextPlugin(parent)
{
    Verbose=1;
    WimeInitialize(0,LOGMARK);
    InitDatabase(NULL,"qim");
    ToggleKeys = GetConvKeyFromResource(QX11Info::display());
    WimePreedit = qim_preedit;
    WimeConvert = qim_convert;
    WimeCommit = qim_commit;
    LOG("parent=%p\n",parent);
}

WimeQimPlugin::~WimeQimPlugin()
{
    LOG("\n");
    WimeFinalize();
}

QInputContext* WimeQimPlugin::create(const QString& key)
{
    QInputContext* c = NULL;
    if(key.toLower() == IdName)
	c = new Qim;
    LOG("key=%s object=%p\n",key.toAscii().data(),c);
    return c;
}

QString WimeQimPlugin::description(const QString& key)
{
    LOG("key=%s\n",key.toAscii().data());
    return "wime";
}

QString WimeQimPlugin::displayName(const QString& key)
{
    LOG("key=%s\n",key.toAscii().data());
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
