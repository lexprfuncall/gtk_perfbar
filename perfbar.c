/*
 * Written by Doug and released to the public domain, as explained at
 * http://creativecommons.org/licenses/publicdomain
 *
 * A gtk look-alike of the original Solaris perfbar
 * 
 * Last update Wed Aug  3 18:51:15 2011  Doug Lea  (dl at altair)
*/

#include <unistd.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/sysinfo.h>

#ifdef LINUX
#include <ctype.h>
#endif

#ifdef SOLARIS
#include <kstat.h>
#endif

struct cpu_times {
  guint64 idle;
  guint64 user;
  guint64 sys;
  guint64 other;
};

typedef struct {
  gboolean ready;
  int ncpus;
  struct cpu_times* current;
  struct cpu_times* prev;
  struct cpu_times* diff;

  /* widgets */
  GtkWidget *app;
  GtkWidget *frame;
  GtkWidget *drawing_area;

  /* graphics */
  GdkRGBA user_color;
  GdkRGBA other_color;
  GdkRGBA sys_color;
  GdkRGBA spacer_color;
  GdkRGBA idle_color;
  gint spacer_width;

} perfbar_panel; 

static perfbar_panel *create_panel(GtkWidget *window, int n);
static void get_times(perfbar_panel* panel);
static void make_diffs(perfbar_panel* panel);
static gint update_cb(gpointer data);
static void draw_func(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);
static guint64 smooth(guint64 current, guint64 prev, guint64 diff);

/* Default dimensions -- scaled (a bit) by ncpus below */
#define DEFAULT_SPACER      1
#define DEFAULT_HEIGHT    128
#define DEFAULT_BAR_WIDTH   7

/* Interval (millisecs) for gtk time-based update */
#define UPDATE_INTERVAL 200

/* Hostname for title bar */
#define MAX_HOSTNAME_LENGTH 256
static char hostname[MAX_HOSTNAME_LENGTH];

static void activate_cb(GtkApplication *app, G_GNUC_UNUSED gpointer user_data) {
  GtkWidget *window;
  perfbar_panel *panel;
  int n = (int)(sysconf(_SC_NPROCESSORS_CONF));

  window = gtk_application_window_new(app);

  gethostname(hostname, MAX_HOSTNAME_LENGTH);
  gtk_window_set_title(GTK_WINDOW(window), hostname);

  panel = create_panel(window, n);
  if (!panel)
    g_error("Can't create widgets!\n");
  get_times(panel);

  gtk_window_set_child(GTK_WINDOW(window), panel->drawing_area);

  gtk_widget_set_visible(window, TRUE);
  panel->ready = TRUE;

  g_timeout_add(UPDATE_INTERVAL, update_cb, panel);
  update_cb(panel);
}

int main(int argc, char **argv) {
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.perfbar", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate_cb), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}

static guint64 smooth(guint64 current, guint64 prev, guint64 diff) {
  //  guint64 d = ((current - prev) + diff) / 2;
  guint64 d = ((current - prev) + 3 * diff) / 4;
  if (d < 0) d = 0;
  return d;
}

static void make_diffs(perfbar_panel* panel) {
  struct cpu_times* tmp;
  int i;
  for (i = 0; i < panel->ncpus; ++i) {
    panel->diff[i].idle  = smooth(panel->current[i].idle,
                                  panel->prev[i].idle,
                                  panel->diff[i].idle);
    panel->diff[i].user  = smooth(panel->current[i].user,
                                  panel->prev[i].user,
                                  panel->diff[i].user);
    panel->diff[i].sys   = smooth(panel->current[i].sys,
                                  panel->prev[i].sys,
                                  panel->diff[i].sys);
    panel->diff[i].other = smooth(panel->current[i].other,
                                  panel->prev[i].other,
                                  panel->diff[i].other);
  }
  
  /* Swap for next time */
  tmp = panel->current;
  panel->current = panel->prev;
  panel->prev = tmp;
}

static perfbar_panel *create_panel(GtkWidget *window, int n) {
  struct cpu_times* tmp;
  gint width;
  gint width_scale;

  perfbar_panel *panel = (perfbar_panel*) g_malloc(sizeof(perfbar_panel));
  panel->app = window;
  panel->ready = FALSE;
  panel->ncpus = n;

  panel->current = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));
  panel->prev = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));
  panel->diff = (struct cpu_times*)(calloc(n, sizeof (struct cpu_times)));

  get_times(panel);
  tmp = panel->current;
  panel->current = panel->prev;
  panel->prev = tmp;
  get_times(panel);

  panel->user_color.red = 0;
  panel->user_color.green = 1.0;
  panel->user_color.blue = 0;
  panel->user_color.alpha = 1.0;

  panel->other_color.red = 0.9;
  panel->other_color.green = 0.9;
  panel->other_color.blue = 0.9;
  panel->other_color.alpha = 1.0;

  panel->sys_color.red = 1.0;
  panel->sys_color.green = 0;
  panel->sys_color.blue = 0;
  panel->sys_color.alpha = 1.0;

  panel->spacer_color.red = 1.0;
  panel->spacer_color.green = 1.0;
  panel->spacer_color.blue = 1.0;
  panel->spacer_color.alpha = 1.0;

  panel->idle_color.red = 0;
  panel->idle_color.green = 0;
  panel->idle_color.blue = 1.0;
  panel->idle_color.alpha = 1.0;

  width_scale = (n <= 2? 8 : n <= 4? 4 : n <= 8? 2 : 1);
  width = (2 * DEFAULT_SPACER + (DEFAULT_SPACER + DEFAULT_BAR_WIDTH) * n) *
    width_scale;
  panel->spacer_width = DEFAULT_SPACER * width_scale;

  /* create widgets */
  panel->drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(panel->drawing_area, width, DEFAULT_HEIGHT);

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(panel->drawing_area),
                                 draw_func, panel, NULL);

  return panel;
}

static void draw_func(G_GNUC_UNUSED GtkDrawingArea *area, cairo_t *cr,
                       int width, int height, gpointer data) {
  perfbar_panel *panel = (perfbar_panel*)data;
  gint x, i;
  gint bar_width, spacer;
  gint h_total;
  int n = panel->ncpus;

  if (!panel->ready) return;

  spacer = panel->spacer_width;
  h_total = height - spacer * 2;
  bar_width = (width - n * spacer) / n;
  if (bar_width <= 0)
    bar_width = 1;

  /* clear top and bottom*/
  cairo_rectangle(cr, 0, 0, width, spacer);
  cairo_fill(cr);
  cairo_rectangle(cr, 0, height - spacer, width, spacer);
  cairo_fill(cr);

  x = 0;
  for (i = 0; i < n; i++) {
    guint64 d_idle = panel->diff[i].idle;
    guint64 d_user = panel->diff[i].user;
    guint64 d_sys = panel->diff[i].sys;
    guint64 d_other = panel->diff[i].other;
    guint64 d_total = d_user + d_other + d_sys + d_idle;
    if (d_total == 0) /* assume idle if all 0 */
      d_idle = d_total = 1;
    
    int spc = spacer;
    if (i == 0) {
      spc = (width - n * bar_width - (n - 1) * spacer) / 2;
      if (spc < 0) spc = 0;
    }
    gdk_cairo_set_source_rgba(cr, &panel->spacer_color);
    cairo_rectangle(cr, x, 0, spc, height);
    cairo_fill(cr);
    x += spc;

    gint y_user, y_other, y_sys, y_idle;
    gint h_user, h_other, h_sys, h_idle, h_sum;
    double scale = (double) h_total / d_total;
    
    h_user = scale * d_user;
    h_sum = h_user;
    
    h_other = scale * d_other;
    h_sum += h_other;
    if (h_sum >= h_total) { /* max out at 100% */
      h_other = h_total - h_user;
      h_sum = h_total;
    }
    
    h_sys = scale * d_sys;
    h_sum += h_sys;
    if (h_sum >= h_total) {
      h_sys = h_total - h_user - h_other;
      h_sum = h_total;
    }
    
    h_idle = scale * d_idle;
    h_sum += h_idle;
    if (h_sum >= h_total) {
      h_sys = h_total - h_user - h_other - h_sys;
      h_sum = h_total;
    }
    
    y_user = h_total - h_user + spacer;
    y_other = y_user - h_other;
    y_sys = y_other - h_sys;
    y_idle = spacer;
    
    if (h_user > 0) {
      gdk_cairo_set_source_rgba(cr, &panel->user_color);
      cairo_rectangle(cr, x, y_user, bar_width, h_user);
      cairo_fill(cr);
    }
    if (h_other > 0) {
      gdk_cairo_set_source_rgba(cr, &panel->other_color);
      cairo_rectangle(cr, x, y_other, bar_width, h_other);
      cairo_fill(cr);
    }
    if (h_sys > 0) {
      gdk_cairo_set_source_rgba(cr, &panel->sys_color);
      cairo_rectangle(cr, x, y_sys, bar_width, h_sys);
      cairo_fill(cr);
    }
    if (h_idle > 0) {
      gdk_cairo_set_source_rgba(cr, &panel->idle_color);
      cairo_rectangle(cr, x, y_idle, bar_width, h_idle);
      cairo_fill(cr);
    }
    x += bar_width;
  }

  /* clear rightmost side */
  if (width - x > 0) {
    gdk_cairo_set_source_rgba(cr, &panel->spacer_color);
    cairo_rectangle(cr, x, 0, width - x, height);
    cairo_fill(cr);
  }
}

gint update_cb(gpointer data) {
  perfbar_panel *panel = (perfbar_panel*)data;
  if (!panel->ready) return TRUE;
  get_times(panel);
  make_diffs(panel);
  gtk_widget_queue_draw(panel->drawing_area);
  return TRUE;
}


#ifdef LINUX
/* PBSIZE should be much more than enough to read lines from proc */
#define PBSIZE 65536
static char pstat_buf[PBSIZE];

static void get_times(perfbar_panel* panel) {
  int i;
  int len;
  char* p;
  int fd;

  fd = open("/proc/stat", O_RDONLY);
  if (fd < 0)
    abort();

  len = read(fd, pstat_buf, PBSIZE-1);
  pstat_buf[len] = 0;
  p = &(pstat_buf[0]);

  if (strncmp(p, "cpu ", 4) == 0) {
    /* skip first line, which is an aggregate of all cpus. */
    while (*p != '\n') p++;
    p++;
  }

  for (i = 0; i < panel->ncpus; ++i) {
    while (*p != ' ' && *p != 0) ++p;
    panel->current[i].user  = strtoull(p, &p, 0);
    panel->current[i].other = strtoull(p, &p, 0);
    panel->current[i].sys   = strtoull(p, &p, 0);
    panel->current[i].idle  = strtoull(p, &p, 0);
    while (*p != '\n' && *p != 0) {
      if (isdigit(*p)) { /* assume 2.6 kernel, with extra values */
        panel->current[i].other += strtoull(p, &p, 0);
        panel->current[i].sys   += strtoull(p, &p, 0); 
        panel->current[i].sys   += strtoull(p, &p, 0); 
      }
      else
        ++p;
    }
  }
  close(fd);
}  

#endif

#ifdef SOLARIS
static void get_times(perfbar_panel* panel) {
  kstat_ctl_t* kstat_control;
  cpu_stat_t stat;
  kstat_t *p;
  int i;

  kstat_control = kstat_open();
  if (kstat_control == NULL)
    abort();

  for (p = kstat_control->kc_chain; p != NULL; p = p->ks_next) {
    if ((strcmp(p->ks_module, "cpu_stat")) == 0 &&
        kstat_read(kstat_control, p, &stat) != -1) {
      i = p->ks_instance;
      panel->current[i].idle = stat.cpu_sysinfo.cpu[CPU_IDLE];
      panel->current[i].user = stat.cpu_sysinfo.cpu[CPU_USER];
      panel->current[i].sys = stat.cpu_sysinfo.cpu[CPU_KERNEL];
      panel->current[i].other = stat.cpu_sysinfo.cpu[CPU_WAIT];
    }
  }
  kstat_close(kstat_control);
}  

#endif
