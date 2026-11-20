// Event loop behaviour test for Qt3: stock select() loop vs. glib loop.
// Non-GUI by default; pass "gui" as argv[1] to also test X11 event handling.
// gio must come before Qt headers: Qt3 #defines "signals"
#include <gio/gio.h>
#include <glib.h>
#include <qapplication.h>
#include <qeventloop.h>
#include <qtimer.h>
#include <qsocketnotifier.h>
#include <qthread.h>
#include <qdatetime.h>
#include <qwidget.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <X11/Xlib.h>

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

static int failures = 0;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fflush(stdout); if (!(cond)) failures++; } while (0)

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
    void readAndDelete(int fd)
    {
        char buf[64];
        ssize_t r = ::read(fd, buf, sizeof buf);
        (void)r;
        selfDeleteCount++;
        delete selfDelete; // delete the notifier from inside its own activation
        selfDelete = 0;
    }
};

class Poster : public QThread
{
public:
    Tester *t;
    void run() override
    {
        msleep(100);
        t->threadPostAt = nowMs();
        QApplication::postEvent(t, new QCustomEvent(QEvent::User + 2));
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

int main(int argc, char **argv)
{
    bool gui = argc > 1 && !strcmp(argv[1], "gui");
    XApp app(argc, argv, gui);
    Tester t;
    gT = &t;
    QEventLoop *el = app.eventLoop();

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
        close(p3[0]);

        // notifier on a closed fd (stock disables it with a warning)
        int p4[2];
        if (pipe(p4) != 0) return 2;
        QSocketNotifier *bad = new QSocketNotifier(p4[0], QSocketNotifier::Read);
        close(p4[0]); close(p4[1]);
        c0 = cpuMs();
        runFor(200);
        cpu = cpuMs() - c0;
        CHECK(cpu < 50, "notifier on closed fd: cpu %.1f ms over 200 ms", cpu);
        delete bad;
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
        Poster p;
        p.t = &t;
        p.start();
        runFor(300);
        p.wait();
        double lat = t.threadRecvAt - t.threadPostAt;
        CHECK(t.threadRecvAt > 0 && lat < 20, "postEvent from QThread latency %.2f ms", lat);
    }
    // 7. glib source coexistence + GDBus
    {
        guint id = g_timeout_add(20, glibTick, &t);
        GError *err = 0;
        GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, 0, &err);
        if (bus) {
            g_dbus_connection_call(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                   "org.freedesktop.DBus", "ListNames", 0,
                                   G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, 2000, 0,
                                   dbusReply, &t);
        } else {
            printf("no session bus: %s\n", err->message);
        }
        runFor(500);
        g_source_remove(id);
        CHECK(t.glibTicks >= 20, "g_timeout_add(20) ticks during Qt loop: %d", t.glibTicks);
        CHECK(t.dbusDone && t.dbusNames > 0, "GDBus ListNames async reply in Qt loop: %d names", t.dbusNames);
        if (bus) g_object_unref(bus);

        // nested g_main_loop_run on the default context: do Qt timers still fire?
        QTimer tm;
        QObject::connect(&tm, SIGNAL(timeout()), &t, SLOT(innerGlib()));
        tm.start(10);
        GMainLoop *ml = g_main_loop_new(0, FALSE);
        g_timeout_add(200, quitGMainLoop, ml);
        g_main_loop_run(ml);
        g_main_loop_unref(ml);
        tm.stop();
        CHECK(t.innerTimerInGlibLoop >= 15, "Qt 10ms timer inside g_main_loop_run(200ms): %d ticks", t.innerTimerInGlibLoop);
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
    {
        int idleRuns = 0;
        guint id = g_idle_add([](gpointer d) -> gboolean { (*(int *)d)++; return G_SOURCE_CONTINUE; }, &idleRuns);
        QTimer z;
        z.start(0);
        runFor(100);
        z.stop();
        g_source_remove(id);
        CHECK(idleRuns > 0, "g_idle_add ran %d times while a Qt 0-timer was running", idleRuns);
    }
    // 10. X11: events already in Xlib's queue when the fd is not readable
    if (gui) {
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
        s.startTimer(20);
        runFor(500);
        double lat = app.xclientAt - s.at;
        CHECK(app.xclient == 1 && lat < 20, "X event queued by XSync processed after %.2f ms", lat);
        w.hide();
    }
    printf("%s: %d failures\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}

#include "looptest.moc"
