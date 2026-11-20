// Event loop behaviour test for Qt3: stock select() loop vs. glib loop.
//
// usage: looptest [gui]          the main suite; "gui" adds the X11 checks
//        looptest recreate [gui] QApplication destroyed and created again
//        looptest early [gui]    QEventLoop created before the QApplication
//        looptest cpu [seconds [gui]]  CPU used by a 10 ms QTimer (default 5 s)
//
// Checks that need the GLib loop (GLib sources, GDBus, ...) are reported as
// SKIP when Qt was built without QT_GLIB_EVENTLOOP.
//
// gio must come before Qt headers: Qt3 #defines "signals"
#include <gio/gio.h>
#include <glib.h>
#if defined(HAVE_PULSE)
#  include <pulse/glib-mainloop.h>
#  include <pulse/context.h>
#endif
#include <qapplication.h>
#include <qeventloop.h>
#include <qtimer.h>
#include <qsocketnotifier.h>
#include <qthread.h>
#include <qdatetime.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qpopupmenu.h>
#include <qstringlist.h>
#include <qptrlist.h>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <X11/Xlib.h>

#if defined(QT_GLIB_EVENTLOOP)
static const bool glibLoop = true;
#else
static const bool glibLoop = false;
#endif

static double nowMs()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static double cpuMs()
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec * 1000.0 + ru.ru_utime.tv_usec / 1000.0 +
           ru.ru_stime.tv_sec * 1000.0 + ru.ru_stime.tv_usec / 1000.0;
}

static int failures = 0, skipped = 0;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fflush(stdout); if (!(cond)) failures++; } while (0)
// a check that needs the GLib loop
#define GCHECK(cond, ...) do { if (glibLoop) CHECK(cond, __VA_ARGS__); else { printf("SKIP "); printf(__VA_ARGS__); printf(" (no GLib loop)\n"); fflush(stdout); skipped++; } } while (0)

// all Qt messages, to check for warnings
static QStringList messages;
static void msgHandler(QtMsgType, const char *msg)
{
    messages.append(msg);
    fprintf(stderr, "%s\n", msg);
}
static int countMessages(const char *substr)
{
    int n = 0;
    for (QStringList::ConstIterator it = messages.begin(); it != messages.end(); ++it)
        if ((*it).contains(substr))
            n++;
    return n;
}

class Tester : public QObject
{
    Q_OBJECT
public:
    int ticks = 0;
    int zeroShots = 0;
    int posted = 0;
    int snReads = 0;
    double threadPostAt = 0, threadRecvAt = 0;
    int glibTicks = 0;
    bool dbusDone = false;
    int dbusNames = 0;
    int nestedTicks = 0;
    int registeredInDispatch = 0;
    int pipefd[2];
    QSocketNotifier *sn = 0;
    QSocketNotifier *selfDelete = 0;
    int selfDeleteCount = 0;
    int innerTimerInGlibLoop = 0;
    int bigReads = 0;
    // qt_mutex stress
    std::atomic<int> *inside = 0;
    int qtMutexChecks = 0, qtMutexViolations = 0;

protected:
    void customEvent(QCustomEvent *e) override
    {
        if (e->type() == QEvent::User + 1)
            posted++;
        else if (e->type() == QEvent::User + 2)
            threadRecvAt = nowMs();
    }

public slots:
    void exitLoop() { qApp->eventLoop()->exitLoop(); }
    void tick() { ticks++; }
    void zeroShot() { zeroShots++; }
    void nestedTick() { nestedTicks++; }
    void innerGlib() { innerTimerInGlibLoop++; }
    void registerDuring()
    {
        // a timer registered while dispatching another timer
        QTimer::singleShot(5, this, SLOT(registeredFired()));
    }
    void registeredFired() { registeredInDispatch++; }
    void readPipe(int fd)
    {
        char buf[64];
        ssize_t n = ::read(fd, buf, sizeof buf);
        if (n > 0)
            snReads++;
    }
    void readBig(int fd)
    {
        char buf[64];
        if (::read(fd, buf, sizeof buf) > 0)
            bigReads++;
    }
    void readAndDelete(int fd)
    {
        char buf[64];
        ssize_t r = ::read(fd, buf, sizeof buf);
        (void)r;
        selfDeleteCount++;
        delete selfDelete; // delete the notifier from inside its own activation
        selfDelete = 0;
    }
    void checkMutex()
    {
        qtMutexChecks++;
        if (inside && inside->load())
            qtMutexViolations++;
    }
};

class Poster : public QThread
{
public:
    Tester *t;
    int delay = 100;
    void run() override
    {
        msleep(delay);
        t->threadPostAt = nowMs();
        QApplication::postEvent(t, new QCustomEvent(QEvent::User + 2));
    }
};

// wakes the GUI thread up after 150 ms
class Waker : public QThread
{
public:
    std::atomic<bool> woken{false};
    void run() override
    {
        msleep(150);
        woken.store(true);
        qApp->wakeUpGuiThread();
    }
};

// takes qApp->lock() in a loop and marks the critical section
class Locker : public QThread
{
public:
    std::atomic<bool> stop{false};
    std::atomic<int> inside{0};
    int rounds = 0;
    void run() override
    {
        while (!stop.load()) {
            qApp->lock();
            inside.store(1);
            double end = nowMs() + 0.05;
            while (nowMs() < end)
                ;
            inside.store(0);
            qApp->unlock();
            rounds++;
            usleep(20);
        }
    }
};

static gboolean glibTick(gpointer data)
{
    ((Tester *)data)->glibTicks++;
    return G_SOURCE_CONTINUE;
}

static void dbusReply(GObject *src, GAsyncResult *res, gpointer data)
{
    Tester *t = (Tester *)data;
    GError *err = 0;
    GVariant *v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    if (v) {
        GVariant *arr = g_variant_get_child_value(v, 0);
        t->dbusNames = (int)g_variant_n_children(arr);
        g_variant_unref(arr);
        g_variant_unref(v);
    } else {
        printf("dbus error: %s\n", err->message);
        g_error_free(err);
    }
    t->dbusDone = true;
}

// run the loop for ms milliseconds
static Tester *gT;
static void runFor(int ms)
{
    QTimer::singleShot(ms, gT, SLOT(exitLoop()));
    qApp->eventLoop()->enterLoop();
}

// run the loop until *flag is set or ms milliseconds have passed
static void runUntil(const bool *flag, int ms)
{
    double end = nowMs() + ms;
    while (!*flag && nowMs() < end)
        qApp->eventLoop()->processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMore);
}

static gboolean quitGMainLoop(gpointer loop)
{
    g_main_loop_quit((GMainLoop *)loop);
    return G_SOURCE_REMOVE;
}

class XApp : public QApplication
{
public:
    XApp(int &argc, char **argv, bool gui) : QApplication(argc, argv, gui) {}
    int xclient = 0;
    double xclientAt = 0;
    bool x11EventFilter(XEvent *e) override
    {
        if (e->type == ClientMessage && e->xclient.data.l[0] == 0x4242) {
            xclient++;
            xclientAt = nowMs();
            return true;
        }
        return false;
    }
};

// X11: an event already in Xlib's queue when the fd is not readable
static void checkQueuedXEvent(XApp &app)
{
    QWidget w;
    w.resize(50, 50);
    w.show();
    runFor(200);
    Display *dpy = w.x11Display();
    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w.winId();
    ev.xclient.format = 32;
    ev.xclient.message_type = XInternAtom(dpy, "QT_LOOPTEST", False);
    ev.xclient.data.l[0] = 0x4242;
    struct Sender : QObject {
        Display *dpy; XEvent ev; double at = 0;
        void timerEvent(QTimerEvent *) override {
            killTimers();
            XSendEvent(dpy, ev.xclient.window, False, NoEventMask, &ev);
            XSync(dpy, False); // reads the event into Xlib's queue; fd is now drained
            at = nowMs();
        }
    } s;
    s.dpy = dpy; s.ev = ev;
    int before = app.xclient;
    s.startTimer(20);
    runFor(500);
    double lat = app.xclientAt - s.at;
    CHECK(app.xclient == before + 1 && lat < 20, "X event queued by XSync processed after %.2f ms", lat);
    w.hide();
}

/*****************************************************************************
  GLib/GIO helpers
 *****************************************************************************/

struct SignalCount { int n = 0; };
static void pingReceived(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                         const gchar *, GVariant *, gpointer data)
{
    ((SignalCount *)data)->n++;
}

struct OwnState { bool acquired = false, lost = false, done = false; };
static void nameAcquired(GDBusConnection *, const gchar *, gpointer data)
{
    OwnState *s = (OwnState *)data;
    s->acquired = s->done = true;
}
static void nameLost(GDBusConnection *, const gchar *, gpointer data)
{
    OwnState *s = (OwnState *)data;
    s->lost = s->done = true;
}

struct TaskState { int result = 0; bool done = false; bool inMainThread = false; pthread_t main; };
static void taskWorker(GTask *task, gpointer, gpointer, GCancellable *)
{
    g_usleep(20000);
    g_task_return_int(task, 42);
}
static void taskDone(GObject *, GAsyncResult *res, gpointer data)
{
    TaskState *s = (TaskState *)data;
    s->result = (int)g_task_propagate_int(G_TASK(res), 0);
    s->inMainThread = pthread_equal(pthread_self(), s->main);
    s->done = true;
}

// QDialog::exec() and QPopupMenu::exec() from inside a GDBus reply callback
struct NestedGui { bool done = false; int dialogResult = -1; bool popupReturned = false; int level = 0; };
static void nestedGuiReply(GObject *src, GAsyncResult *res, gpointer data)
{
    NestedGui *n = (NestedGui *)data;
    GVariant *v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, 0);
    if (v)
        g_variant_unref(v);
    n->level = qApp->eventLoop()->loopLevel();
    {
        QDialog d;
        d.resize(80, 40);
        QTimer::singleShot(150, &d, SLOT(accept()));
        n->dialogResult = d.exec();
    }
    {
        QPopupMenu m;
        m.insertItem("item");
        QTimer::singleShot(150, &m, SLOT(hide()));
        m.exec(QPoint(20, 20));
        n->popupReturned = true;
    }
    n->done = true;
}

static gboolean exitFromGlib(gpointer)
{
    qApp->exit(7);
    return G_SOURCE_REMOVE;
}

#if defined(HAVE_PULSE)
static void paState(pa_context *c, void *data)
{
    *(pa_context_state_t *)data = pa_context_get_state(c);
}
#endif

/*****************************************************************************
  Modes
 *****************************************************************************/

static int cpuMode(int argc, char **argv)
{
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    bool gui = argc > 3 && !strcmp(argv[3], "gui");
    QApplication app(argc, argv, gui);
    Tester t;
    QTimer tm;
    QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(tick()));
    tm.start(10);
    QTimer::singleShot(secs * 1000, &app, SLOT(quit()));
    struct rusage r0, r1;
    getrusage(RUSAGE_SELF, &r0);
    app.exec();
    getrusage(RUSAGE_SELF, &r1);
    double user = (r1.ru_utime.tv_sec - r0.ru_utime.tv_sec) * 1000.0 + (r1.ru_utime.tv_usec - r0.ru_utime.tv_usec) / 1000.0;
    double sys = (r1.ru_stime.tv_sec - r0.ru_stime.tv_sec) * 1000.0 + (r1.ru_stime.tv_usec - r0.ru_stime.tv_usec) / 1000.0;
    printf("10ms QTimer for %d s: %d ticks, cpu %.1f ms (user %.1f, sys %.1f) = %.3f ms/s, %ld voluntary context switches\n",
           secs, t.ticks, user + sys, user, sys, (user + sys) / secs, r1.ru_nvcsw - r0.ru_nvcsw);
    return 0;
}

// QApplication destroyed, the default GMainContext iterated, a new one created
static int recreateMode(int argc, char **argv, bool gui)
{
    for (int round = 1; round <= 3; ++round) {
        {
            XApp app(argc, argv, gui);
            Tester t;
            gT = &t;
            QTimer tm;
            QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(tick()));
            tm.start(10);
            guint id = glibLoop ? g_timeout_add(10, glibTick, &t) : 0;
            QTimer::singleShot(200, &app, SLOT(quit()));
            int sock[2];
            if (pipe(sock) != 0)
                return 2;
            QSocketNotifier n(sock[0], QSocketNotifier::Read);
            QObject::connect(&n, SIGNAL(activated(int)), &t, SLOT(readPipe(int)));
            ssize_t w = write(sock[1], "x", 1); (void)w;
            if (gui) {
                QWidget wid;
                wid.resize(40, 40);
                wid.show();
                app.exec();
            } else {
                app.exec();
            }
            if (id)
                g_source_remove(id);
            close(sock[0]); close(sock[1]);
            CHECK(t.ticks >= 15 && t.snReads == 1, "round %d: Qt timer %d ticks, notifier %d", round, t.ticks, t.snReads);
            GCHECK(t.glibTicks >= 15, "round %d: g_timeout_add %d ticks", round, t.glibTicks);
        }
        // the Qt sources must be gone with the event loop
        int idle = 0;
        guint id = g_idle_add([](gpointer d) -> gboolean { (*(int *)d)++; return G_SOURCE_REMOVE; }, &idle);
        for (int i = 0; i < 5; i++)
            g_main_context_iteration(0, FALSE);
        if (!idle)
            g_source_remove(id);
        CHECK(idle == 1, "round %d: default context iterated after ~QApplication (idle ran %d)", round, idle);
    }
    printf("%s: %d failures, %d skipped\n", failures ? "FAILED" : "OK", failures, skipped);
    return failures ? 1 : 0;
}

// QEventLoop created before the QApplication (before the display is opened)
static int earlyMode(int argc, char **argv, bool gui)
{
    QEventLoop *loop = new QEventLoop(0, "early event loop");
    {
        XApp app(argc, argv, gui);
        CHECK(app.eventLoop() == loop, "QApplication uses the event loop created before it");
        Tester t;
        gT = &t;
        QTimer tm;
        QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(tick()));
        tm.start(10);
        runFor(200);
        tm.stop();
        CHECK(t.ticks >= 15, "early loop: Qt timer %d ticks", t.ticks);
        if (gui)
            checkQueuedXEvent(app);
    }
    delete loop;
    printf("%s: %d failures, %d skipped\n", failures ? "FAILED" : "OK", failures, skipped);
    return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
    qInstallMsgHandler(msgHandler);
    const char *mode = argc > 1 ? argv[1] : "";
    if (!strcmp(mode, "cpu"))
        return cpuMode(argc, argv);
    if (!strcmp(mode, "recreate"))
        return recreateMode(argc, argv, argc > 2 && !strcmp(argv[2], "gui"));
    if (!strcmp(mode, "early"))
        return earlyMode(argc, argv, argc > 2 && !strcmp(argv[2], "gui"));

    bool gui = !strcmp(mode, "gui");
    printf("Qt event loop: %s\n", glibLoop ? "GLib" : "select()");
    XApp app(argc, argv, gui);
    Tester t;
    gT = &t;
    QEventLoop *el = app.eventLoop();
    GError *busErr = 0;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, 0, &busErr);
    if (!bus) {
        printf("no session bus: %s\n", busErr->message);
        g_error_free(busErr);
    }

    // 1. timers
    {
        QTimer tm;
        QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(tick()));
        tm.start(10);
        double c0 = cpuMs();
        runFor(500);
        tm.stop();
        double cpu = cpuMs() - c0;
        CHECK(t.ticks >= 45 && t.ticks <= 51, "10ms timer over 500ms: %d ticks", t.ticks);
        CHECK(cpu < 50, "CPU used while idle with 10ms timer: %.1f ms", cpu);
    }
    // 2. zero timer + registration during dispatch
    {
        QTimer::singleShot(0, &t, SLOT(zeroShot()));
        QTimer::singleShot(1, &t, SLOT(registerDuring()));
        runFor(50);
        CHECK(t.zeroShots == 1, "singleShot(0) fired %d times", t.zeroShots);
        CHECK(t.registeredInDispatch == 1, "timer registered during dispatch fired %d", t.registeredInDispatch);
    }
    // 3. posted events
    {
        for (int i = 0; i < 10; i++)
            QApplication::postEvent(&t, new QCustomEvent(QEvent::User + 1));
        runFor(20);
        CHECK(t.posted == 10, "posted events delivered: %d", t.posted);
    }
    // 4. socket notifiers
    {
        if (pipe(t.pipefd) != 0) return 2;
        t.sn = new QSocketNotifier(t.pipefd[0], QSocketNotifier::Read);
        QObject::connect(t.sn, SIGNAL(activated(int)), &t, SLOT(readPipe(int)));
        ssize_t w = write(t.pipefd[1], "x", 1); (void)w;
        runFor(20);
        CHECK(t.snReads == 1, "read notifier activations: %d", t.snReads);
        t.sn->setEnabled(false);
        w = write(t.pipefd[1], "y", 1);
        runFor(20);
        CHECK(t.snReads == 1, "disabled notifier not activated (%d)", t.snReads);
        t.sn->setEnabled(true);
        runFor(20);
        CHECK(t.snReads == 2, "re-enabled notifier activated (%d)", t.snReads);
        delete t.sn;

        int p2[2];
        if (pipe(p2) != 0) return 2;
        t.selfDelete = new QSocketNotifier(p2[0], QSocketNotifier::Read);
        QObject::connect(t.selfDelete, SIGNAL(activated(int)), &t, SLOT(readAndDelete(int)));
        w = write(p2[1], "z", 1);
        runFor(20);
        CHECK(t.selfDeleteCount == 1, "notifier deleted inside its slot (%d activations, no crash)", t.selfDeleteCount);
        close(p2[0]); close(p2[1]);

        // ExcludeSocketNotifiers with a hung-up pipe: must not spin
        int p3[2];
        if (pipe(p3) != 0) return 2;
        QSocketNotifier hup(p3[0], QSocketNotifier::Read);
        close(p3[1]);
        double c0 = cpuMs();
        int iters = 0;
        double end = nowMs() + 200;
        while (nowMs() < end) {
            // a 10ms timer keeps WaitForMore from blocking forever
            QTimer guard; guard.start(10, true);
            el->processEvents(QEventLoop::ExcludeSocketNotifiers | QEventLoop::WaitForMore);
            iters++;
        }
        double cpu = cpuMs() - c0;
        CHECK(iters < 40, "ExcludeSocketNotifiers+WaitForMore with HUP fd: %d iterations in 200ms, cpu %.1f ms", iters, cpu);
        CHECK(cpu < 20, "ExcludeSocketNotifiers+WaitForMore with HUP fd: cpu %.1f ms over 200 ms", cpu);
        hup.setEnabled(false); // the fd number is reused below
        close(p3[0]);

        // notifier on a closed fd (stock disables it with a warning)
        int p4[2];
        if (pipe(p4) != 0) return 2;
        QSocketNotifier *bad = new QSocketNotifier(p4[0], QSocketNotifier::Read);
        int badfd = p4[0];
        close(p4[0]); close(p4[1]);
        int warnings = countMessages("invalid socket");
        c0 = cpuMs();
        runFor(200);
        cpu = cpuMs() - c0;
        CHECK(cpu < 50, "notifier on closed fd: cpu %.1f ms over 200 ms", cpu);
        CHECK(!bad->isEnabled() && countMessages("invalid socket") == warnings + 1,
              "notifier on closed fd %d disabled (%d) with one warning (%d)", badfd,
              !bad->isEnabled(), countMessages("invalid socket") - warnings);
        delete bad;
    }
    // 4b. ExcludeTimers (X11 only flag 0x08, used by the clipboard) with a due
    // timer: must block until woken up instead of spinning
    {
        QTimer due;
        QObject::connect(&due, SIGNAL(timeout()), &t, SLOT(tick()));
        due.start(1);
        int ticks = t.ticks;
        Waker waker;
        waker.start();
        double c0 = cpuMs();
        double end = nowMs() + 1000;
        int iters = 0;
        while (!waker.woken.load() && nowMs() < end) {
            el->processEvents(QEventLoop::ExcludeUserInput | QEventLoop::ExcludeSocketNotifiers |
                              QEventLoop::WaitForMore | 0x08);
            iters++;
        }
        double cpu = cpuMs() - c0;
        waker.wait();
        due.stop();
        CHECK(waker.woken.load() && iters <= 3 && cpu < 20 && t.ticks == ticks,
              "ExcludeTimers with a due timer: %d iterations, cpu %.1f ms, %d timer events, woken: %d",
              iters, cpu, t.ticks - ticks, waker.woken.load());
    }
    // 5. nested loops
    {
        QTimer tm;
        QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(nestedTick()));
        tm.start(10);
        int lvl = -1;
        // enter a nested loop from inside a timer: use a lambda-free approach
        // emulate QDialog::exec(): nested enterLoop while outer is active
        struct Inner : QObject {
            int *lvl;
            void timerEvent(QTimerEvent *) override {
                killTimers();
                *lvl = qApp->eventLoop()->loopLevel();
                QTimer::singleShot(100, gT, SLOT(exitLoop()));
                qApp->eventLoop()->enterLoop();
            }
        } inner;
        inner.lvl = &lvl;
        inner.startTimer(1);
        runFor(300);
        tm.stop();
        CHECK(lvl == 2 || lvl == 1, "nested loop level seen %d", lvl);
        CHECK(t.nestedTicks >= 25, "outer 10ms timer kept running across nested loop: %d ticks in 300ms", t.nestedTicks);
    }
    // 6. wakeUp from another thread
    {
        t.threadRecvAt = 0;
        Poster p;
        p.t = &t;
        p.start();
        runFor(300);
        p.wait();
        double lat = t.threadRecvAt - t.threadPostAt;
        CHECK(t.threadRecvAt > 0 && lat < 20, "postEvent from QThread latency %.2f ms", lat);
    }
    // 7. glib source coexistence + GDBus
    if (glibLoop) {
        guint id = g_timeout_add(20, glibTick, &t);
        if (bus)
            g_dbus_connection_call(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                   "org.freedesktop.DBus", "ListNames", 0,
                                   G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, 2000, 0,
                                   dbusReply, &t);
        runFor(500);
        g_source_remove(id);
        GCHECK(t.glibTicks >= 20, "g_timeout_add(20) ticks during Qt loop: %d", t.glibTicks);
        GCHECK(t.dbusDone && t.dbusNames > 0, "GDBus ListNames async reply in Qt loop: %d names", t.dbusNames);

        // nested g_main_loop_run on the default context: do Qt timers still fire?
        QTimer tm;
        QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(innerGlib()));
        tm.start(10);
        GMainLoop *ml = g_main_loop_new(0, FALSE);
        g_timeout_add(200, quitGMainLoop, ml);
        g_main_loop_run(ml);
        g_main_loop_unref(ml);
        tm.stop();
        GCHECK(t.innerTimerInGlibLoop >= 15, "Qt 10ms timer inside g_main_loop_run(200ms): %d ticks", t.innerTimerInGlibLoop);
    } else {
        GCHECK(false, "g_timeout_add(20) ticks during Qt loop");
        GCHECK(false, "GDBus ListNames async reply in Qt loop");
        GCHECK(false, "Qt 10ms timer inside g_main_loop_run(200ms)");
    }
    // 8. processEvents(flags, maxtime) with a zero timer pending: must return quickly
    {
        QTimer z;
        z.start(0);
        double t0 = nowMs();
        el->processEvents(QEventLoop::AllEvents, 200);
        double dt = nowMs() - t0;
        z.stop();
        CHECK(dt < 50, "processEvents(AllEvents, 200) with a running 0-timer returned after %.1f ms", dt);
    }
    // 9. idle glib source vs. Qt 0-timer (starvation)
    if (glibLoop) {
        int idleRuns = 0;
        guint id = g_idle_add([](gpointer d) -> gboolean { (*(int *)d)++; return G_SOURCE_CONTINUE; }, &idleRuns);
        QTimer z;
        QObject::connect(&z, SIGNAL(timeout()), &t, SLOT(zeroShot()));
        int zeros = t.zeroShots;
        z.start(0);
        runFor(100);
        z.stop();
        g_source_remove(id);
        GCHECK(idleRuns > 100 && t.zeroShots - zeros > 100,
               "g_idle_add ran %d times while a Qt 0-timer ran %d times", idleRuns, t.zeroShots - zeros);
    } else {
        GCHECK(false, "g_idle_add next to a Qt 0-timer");
    }
    // 10. qt_mutex is held while Qt and GLib sources are dispatched
    {
        Locker locker;
        int glibChecks = 0, glibViolations = 0;
        struct GState { Locker *l; int *checks, *violations; } gs = { &locker, &glibChecks, &glibViolations };
        guint id = 0;
        if (glibLoop)
            id = g_timeout_add(1, [](gpointer d) -> gboolean {
                GState *s = (GState *)d;
                (*s->checks)++;
                if (s->l->inside.load())
                    (*s->violations)++;
                return G_SOURCE_CONTINUE;
            }, &gs);
        t.inside = &locker.inside;
        QTimer qt;
        QObject::connect(&qt, SIGNAL(timeout()), &t, SLOT(checkMutex()));
        qt.start(1);
        int mutexWarnings = countMessages("QMutex");
        locker.start();
        runFor(300);
        locker.stop.store(true);
        locker.wait();
        qt.stop();
        if (id)
            g_source_remove(id);
        t.inside = 0;
        CHECK(t.qtMutexChecks > 50 && t.qtMutexViolations == 0 && locker.rounds > 10,
              "qt_mutex held in Qt timers: %d checks, %d violations, thread got the lock %d times",
              t.qtMutexChecks, t.qtMutexViolations, locker.rounds);
        GCHECK(glibChecks > 50 && glibViolations == 0,
               "qt_mutex held in GLib callbacks: %d checks, %d violations", glibChecks, glibViolations);
        CHECK(countMessages("QMutex") == mutexWarnings, "no QMutex warnings (%d)", countMessages("QMutex") - mutexWarnings);
    }
    // 11. a GDBus signal arriving during processEvents(ExcludeUserInput|
    // ExcludeSocketNotifiers) is dispatched only afterwards
    if (glibLoop && bus) {
        SignalCount count;
        guint sub = g_dbus_connection_signal_subscribe(bus, 0, "org.kde3revival.LoopTest", "Ping",
                                                       "/org/kde3revival/LoopTest", 0,
                                                       G_DBUS_SIGNAL_FLAGS_NONE, pingReceived, &count, 0);
        runFor(50); // let the match rule reach the bus
        g_dbus_connection_emit_signal(bus, 0, "/org/kde3revival/LoopTest", "org.kde3revival.LoopTest",
                                      "Ping", 0, 0);
        g_dbus_connection_flush_sync(bus, 0, 0);
        double end = nowMs() + 300;
        while (nowMs() < end) {
            QTimer guard; guard.start(10, true);
            el->processEvents(QEventLoop::ExcludeUserInput | QEventLoop::ExcludeSocketNotifiers |
                              QEventLoop::WaitForMore);
        }
        int during = count.n;
        runFor(300);
        g_dbus_connection_signal_unsubscribe(bus, sub);
        GCHECK(during == 0 && count.n == 1, "GDBus signal during an Exclude* wait: %d dispatched during, %d after",
               during, count.n);
    } else {
        GCHECK(false, "GDBus signal during an Exclude* wait");
    }
    // 12. quit() from a GLib callback inside nested loops
    if (glibLoop) {
        struct Nester : QObject {
            int level = 0; bool returned = false;
            void timerEvent(QTimerEvent *) override {
                killTimers();
                level = qApp->eventLoop()->loopLevel();
                g_timeout_add(30, exitFromGlib, 0);
                qApp->eventLoop()->enterLoop();
                returned = true;
            }
        } nester;
        nester.startTimer(10);
        double t0 = nowMs();
        int rc = el->exec();
        // level = loop level of the outer loop, the inner one is level 2
        GCHECK(rc == 7 && nester.returned && nester.level == 1 && nowMs() - t0 < 1000,
               "exit(7) from a GLib callback in a nested loop: exec() = %d, outer level %d, inner returned %d",
               rc, nester.level, nester.returned);
    } else {
        GCHECK(false, "exit() from a GLib callback in a nested loop");
    }
    // 13. more than FD_SETSIZE fds with socket notifiers
    if (glibLoop) {
        struct rlimit rl;
        getrlimit(RLIMIT_NOFILE, &rl);
        if (rl.rlim_cur < 4096 && rl.rlim_max >= 4096) {
            rl.rlim_cur = 4096;
            setrlimit(RLIMIT_NOFILE, &rl);
        }
        QPtrList<QSocketNotifier> notifiers;
        notifiers.setAutoDelete(true);
        int pipes[700][2];
        int n = 0, maxfd = 0;
        for (; n < 700; n++) {
            if (pipe(pipes[n]) != 0)
                break;
            maxfd = QMAX(maxfd, pipes[n][1]);
            QSocketNotifier *sn = new QSocketNotifier(pipes[n][0], QSocketNotifier::Read);
            QObject::connect(sn, SIGNAL(activated(int)), &t, SLOT(readBig(int)));
            notifiers.append(sn);
        }
        for (int i = n - 10; i < n; i++) {
            ssize_t w = write(pipes[i][1], "x", 1); (void)w;
        }
        runFor(50);
        int normal = t.bigReads;
        // and through the Qt-only path
        for (int i = n - 20; i < n - 10; i++) {
            ssize_t w = write(pipes[i][1], "x", 1); (void)w;
        }
        for (int i = 0; i < 5; i++)
            el->processEvents(QEventLoop::ExcludeUserInput);
        int qtOnly = t.bigReads - normal;
        notifiers.clear();
        for (int i = 0; i < n; i++) {
            close(pipes[i][0]); close(pipes[i][1]);
        }
        GCHECK(maxfd > 1024 && normal == 10 && qtOnly == 10,
               "%d notifiers, fds up to %d: %d activations, %d with ExcludeUserInput", n, maxfd, normal, qtOnly);
    } else {
        GCHECK(false, "notifiers on fds above FD_SETSIZE");
    }
    // 14. libpulse's GLib main loop
#if defined(HAVE_PULSE)
    if (glibLoop) {
        pa_glib_mainloop *m = pa_glib_mainloop_new(0);
        pa_context *c = pa_context_new(pa_glib_mainloop_get_api(m), "looptest");
        pa_context_state_t state = PA_CONTEXT_UNCONNECTED;
        pa_context_set_state_callback(c, paState, &state);
        pa_context_connect(c, 0, PA_CONTEXT_NOAUTOSPAWN, 0);
        double end = nowMs() + 3000;
        while (state != PA_CONTEXT_READY && state != PA_CONTEXT_FAILED && state != PA_CONTEXT_TERMINATED &&
               nowMs() < end)
            el->processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMore);
        pa_context_state_t reached = state;
        pa_context_disconnect(c);
        pa_context_unref(c);
        pa_glib_mainloop_free(m);
        GCHECK(reached == PA_CONTEXT_READY, "pa_glib_mainloop: context state %d (READY = %d)", reached, PA_CONTEXT_READY);
    } else {
        GCHECK(false, "pa_glib_mainloop");
    }
#else
    printf("SKIP pa_glib_mainloop (built without libpulse-mainloop-glib)\n");
    skipped++;
#endif
    // 15. g_bus_own_name()
    if (glibLoop && bus) {
        OwnState s;
        char name[64];
        snprintf(name, sizeof name, "org.kde3revival.LoopTest.p%d", (int)getpid());
        guint own = g_bus_own_name(G_BUS_TYPE_SESSION, name, G_BUS_NAME_OWNER_FLAGS_NONE, 0,
                                   nameAcquired, nameLost, &s, 0);
        runUntil(&s.done, 3000);
        g_bus_unown_name(own);
        GCHECK(s.acquired, "g_bus_own_name(%s): acquired %d, lost %d", name, s.acquired, s.lost);
    } else {
        GCHECK(false, "g_bus_own_name()");
    }
    // 16. GTask result from a worker thread
    if (glibLoop) {
        TaskState s;
        s.main = pthread_self();
        GTask *task = g_task_new(0, 0, taskDone, &s);
        g_task_run_in_thread(task, taskWorker);
        g_object_unref(task);
        runUntil(&s.done, 3000);
        GCHECK(s.done && s.result == 42 && s.inMainThread, "GTask from a worker thread: result %d, callback in GUI thread %d",
               s.result, s.inMainThread);
    } else {
        GCHECK(false, "GTask from a worker thread");
    }
    // 17. X11: an event already in Xlib's queue when the fd is not readable
    if (gui)
        checkQueuedXEvent(app);
    // 18. QDialog::exec() and QPopupMenu::exec() inside a GDBus callback
    if (gui && glibLoop && bus) {
        NestedGui n;
        g_dbus_connection_call(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                               "org.freedesktop.DBus", "GetId", 0, 0, G_DBUS_CALL_FLAGS_NONE, 2000, 0,
                               nestedGuiReply, &n);
        runUntil(&n.done, 5000);
        GCHECK(n.done && n.dialogResult == QDialog::Accepted && n.popupReturned,
               "QDialog::exec() (result %d) and QPopupMenu::exec() (returned %d) in a GDBus callback",
               n.dialogResult, n.popupReturned);
    } else if (gui) {
        GCHECK(false, "QDialog/QPopupMenu exec() in a GDBus callback");
    }

    if (bus)
        g_object_unref(bus);
    printf("%s: %d failures, %d skipped\n", failures ? "FAILED" : "OK", failures, skipped);
    return failures ? 1 : 0;
}

#include "looptest.moc"
