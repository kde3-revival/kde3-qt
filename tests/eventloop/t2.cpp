#include <glib.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qeventloop.h>
#include <stdio.h>
#include <string.h>
static gboolean idle(gpointer d) { (*(int*)d)++; return G_SOURCE_CONTINUE; }
int main(int argc, char **argv)
{
    {
        QApplication app(argc, argv, false);
        int n = 0;
        guint id = g_idle_add(idle, &n);
        QTimer t; t.start(100, true);
        QObject::connect(&t, SIGNAL(timeout()), &app, SLOT(quit()));
        app.exec();
        g_source_remove(id);
        printf("idle ran %d times in 100ms with no Qt 0-timer\n", n);
        GSource *s = g_main_context_find_source_by_id(0, 1);
        printf("source id 1 after loop: %p\n", (void*)s);
    }
    printf("QApplication destroyed; iterating default GMainContext...\n"); fflush(stdout);
    for (int i = 0; i < 3; i++) g_main_context_iteration(0, FALSE);
    printf("survived\n");
    return 0;
}
