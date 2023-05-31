#ifndef QWIME_H
#define QWIME_H

#include <QtGlobal>
#include "exe/at.h"

#if QT_VERSION >= 0x050000
#include <QInputMethodEvent>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatforminputcontextplugin_p.h>
#include <QLocale>
typedef QPlatformInputContextPlugin PluginBase;
typedef QPlatformInputContext InputContextBase;
#else
#include <QInputContextPlugin>
#include <QInputContext>
typedef QInputContextPlugin PluginBase;
typedef QInputContext InputContextBase;
#endif

class QWimePlugin : public PluginBase
{
#if QT_VERSION >= 0x050000
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformInputContextFactoryInterface_iid
		      FILE "wime.json")
#endif

public:
    QWimePlugin(QObject* parent=nullptr);
    virtual ~QWimePlugin();
#if QT_VERSION >= 0x050000
    virtual InputContextBase* create(const QString& key, const QStringList& paramlist);
#else
    virtual InputContextBase* create(const QString& key);
    virtual QString description(const QString& key);
    virtual QString displayName(const QString& key);
    virtual QStringList keys() const;
    virtual QStringList languages(const QString& key);
#endif
};

class QWime : public InputContextBase
{
    QObject* FocusObj; //4では使わない。
    int WimeCxn;
    int ServerLevel;
    
    void create_wime_context();
    void replace_context();
public:
    QWime(QObject* parent_qt4=nullptr);
    virtual ~QWime();
    QObject* FocusObject();
    void SendEvToFocusObj(QInputMethodEvent* ev);

    virtual bool filterEvent(const QEvent* event);
    virtual void reset();
    virtual void update(); //5にはない。
#if QT_VERSION >= 0x050000
    virtual void update(Qt::InputMethodQueries);
    virtual void setFocusObject(QObject* object);
    virtual bool isValid() const;
    //virtual bool isInputPanelVisible() const;
    //virtual void showInputPanel();
    //virtual void hideInputPanel();
    //virtual bool isAnimating() const;
    //virtual void invokeAction(QInputMethod::Action action, int cursorPosition);
    //virtual Qt::LayoutDirection inputDirection() const;
    //virtual void commit(){}
    //virtual QRectF keyboardRect() const;
    //virtual QLocale locale() const;
#else
    virtual QString identifierName();
    virtual bool isComposing() const;
    virtual QString language();
    //virtual QList<QAction *> actions()
    //virtual QFont font() const
    //virtual void mouseHandler(int x, QMouseEvent* event);
    //virtual void setFocusWidget(QWidget* widget)
    //virtual bool symbianFilterEvent(QWidget* keywidget, const QSymbianEvent* event)
    //virtual void widgetDestroyed(QWidget* widget)
    //virtual bool x11FilterEvent (QWidget* keywidget, XEvent* event);
#endif
};

#endif

//(C) 2020 thomas
