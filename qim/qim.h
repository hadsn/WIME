#include <QInputContextPlugin>
#include <QInputContext>

class WimeQimPlugin:public QInputContextPlugin{
public:
    WimeQimPlugin(QObject* parent=NULL);
    virtual ~WimeQimPlugin();

    virtual QInputContext* create(const QString& key);
    virtual QString description(const QString& key);
    virtual QString displayName(const QString& key);
    virtual QStringList keys() const;
    virtual QStringList languages(const QString& key);
};

class Qim:public QInputContext{
    int WimeCxn;
    bool Enabled;
    int ServerLevel;

    void create_wime_context();
    void replace_context();
public:
    Qim(QObject* parent=NULL);
    virtual ~Qim();
    virtual QString identifierName();
    virtual bool isComposing() const;
    virtual QString language();
    virtual void reset();
    //virtual QList<QAction *> 	actions ()
    virtual bool filterEvent(const QEvent* event);
    //QWidget * 	focusWidget () const
    //virtual QFont 	font () const
    //virtual void 	mouseHandler ( int x, QMouseEvent * event );
    //void 	sendEvent ( const QInputMethodEvent & event )
    //virtual void 	setFocusWidget ( QWidget * widget )
    //QTextFormat 	standardFormat ( StandardFormat s ) const
    //virtual bool 	symbianFilterEvent ( QWidget * keywidget, const QSymbianEvent * event )
    virtual void update();
    //virtual void 	widgetDestroyed ( QWidget * widget )
    //virtual bool 	x11FilterEvent ( QWidget * keywidget, XEvent * event );
};

//(C) 2011 thomas
