#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sys/statvfs.h>

#define TOTAL_FRAMES 5

typedef struct {
    XfcePanelPlugin *plugin;
    GtkWidget *ebox;
    GtkWidget *box;
    GtkWidget *image;
    GtkWidget *label;
    GdkPixbuf *frames_dark[TOTAL_FRAMES];
    GdkPixbuf *frames_light[TOTAL_FRAMES];
    GdkPixbuf *cache_dark[TOTAL_FRAMES];
    GdkPixbuf *cache_light[TOTAL_FRAMES];
    gint cached_size;
    gint last_delay;
    gboolean is_dark;
    gint current_frame;
    gint cpu_usage;
    gint gpu_usage;
    guint64 gpu_mem_used;
    guint64 gpu_mem_total;
    gdouble mem_total_mb;
    gdouble mem_avail_mb;
    gint disk_pct;
    guint anim_timeout_id;
    guint cpu_timeout_id;
    guint stats_timeout_id;
    guint64 prev_idle;
    guint64 prev_total;
    gchar *resource_path;
} RunCatPlugin;

static void runcat_free(XfcePanelPlugin *plugin, gpointer data);
static gboolean runcat_update_animation(gpointer data);
static gboolean runcat_update_cpu(gpointer data);
static gboolean runcat_update_stats(gpointer data);
static void runcat_update_tooltip(RunCatPlugin *rc);
static void runcat_load_frames(RunCatPlugin *rc);
static void runcat_update_theme(RunCatPlugin *rc);
static gint runcat_get_cpu_delay(RunCatPlugin *rc);

static gint runcat_get_cpu_delay(RunCatPlugin *rc) {
    if (rc->cpu_usage < 10) return 100;   // 10 FPS
    if (rc->cpu_usage < 20) return 90;
    if (rc->cpu_usage < 40) return 80;
    if (rc->cpu_usage < 50) return 70;
    if (rc->cpu_usage < 70) return 60;
    return 50; // 20 FPS
}

static void runcat_ensure_cache(RunCatPlugin *rc, gint target) {
    if (rc->cached_size == target) return;
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        if (rc->cache_dark[i]) { g_object_unref(rc->cache_dark[i]); rc->cache_dark[i] = NULL; }
        if (rc->cache_light[i]) { g_object_unref(rc->cache_light[i]); rc->cache_light[i] = NULL; }
        if (rc->frames_dark[i])
            rc->cache_dark[i] = gdk_pixbuf_scale_simple(rc->frames_dark[i], target, target, GDK_INTERP_BILINEAR);
        if (rc->frames_light[i])
            rc->cache_light[i] = gdk_pixbuf_scale_simple(rc->frames_light[i], target, target, GDK_INTERP_BILINEAR);
    }
    rc->cached_size = target;
}

static void runcat_load_frames(RunCatPlugin *rc) {
    (void)rc->is_dark; // suppress unused warning, prefix used via frames arrays
    gchar *datadirs[] = {
        g_build_filename(g_get_user_data_dir(), "arch-run-cat", "cat", NULL),
        g_build_filename(DATADIR, "arch-run-cat", "cat", NULL),
        g_build_filename("/usr/share/arch-run-cat/cat", NULL),
        NULL
    };

    // try installed path first, then relative for dev
    gchar *base = NULL;
    for (int i = 0; datadirs[i]; i++) {
        gchar *test = g_build_filename(datadirs[i], "dark_cat_0.png", NULL);
        if (g_file_test(test, G_FILE_TEST_EXISTS)) {
            base = g_strdup(datadirs[i]);
            g_free(test);
            break;
        }
        g_free(test);
    }
    if (!base) {
        // dev fallback: relative to plugin .so location not reliable, try /usr/share
        base = g_strdup("/usr/share/arch-run-cat/cat");
    }

    for (int i = 0; i < TOTAL_FRAMES; i++) {
        gchar *fname_dark = g_strdup_printf("dark_cat_%d.png", i);
        gchar *fname_light = g_strdup_printf("light_cat_%d.png", i);
        gchar *path_dark = g_build_filename(base, fname_dark, NULL);
        gchar *path_light = g_build_filename(base, fname_light, NULL);
        if (rc->frames_dark[i]) g_object_unref(rc->frames_dark[i]);
        if (rc->frames_light[i]) g_object_unref(rc->frames_light[i]);
        GError *err = NULL;
        rc->frames_dark[i] = gdk_pixbuf_new_from_file(path_dark, &err);
        if (err) { g_clear_error(&err); rc->frames_dark[i] = NULL; }
        err = NULL;
        rc->frames_light[i] = gdk_pixbuf_new_from_file(path_light, &err);
        if (err) { g_clear_error(&err); rc->frames_light[i] = NULL; }
        g_free(fname_dark); g_free(fname_light);
        g_free(path_dark); g_free(path_light);
    }

    // fallback to relative resources/cat for dev build (when running from build dir)
    if (!rc->frames_dark[0]) {
        gchar *rel = g_build_filename("resources", "cat", NULL);
        if (g_file_test(rel, G_FILE_TEST_IS_DIR)) {
            g_free(base);
            base = g_strdup(rel);
            for (int i = 0; i < TOTAL_FRAMES; i++) {
                gchar *p = g_build_filename(base, g_strdup_printf("dark_cat_%d.png", i), NULL);
                if (!rc->frames_dark[i]) rc->frames_dark[i] = gdk_pixbuf_new_from_file(p, NULL);
                g_free(p);
                p = g_build_filename(base, g_strdup_printf("light_cat_%d.png", i), NULL);
                if (!rc->frames_light[i]) rc->frames_light[i] = gdk_pixbuf_new_from_file(p, NULL);
                g_free(p);
            }
        }
        g_free(rel);
    }

    for (int i = 0; datadirs[i]; i++) g_free(datadirs[i]);
    g_free(base);
    g_free(rc->resource_path);
    rc->resource_path = NULL;
}

static void runcat_update_theme(RunCatPlugin *rc) {
    const gchar *env = g_getenv("RUN_CAT_THEME");
    if (env && *env) {
        rc->is_dark = (g_ascii_strcasecmp(env, "dark") == 0);
        return;
    }
    if (xfconf_init(NULL)) {
        XfconfChannel *ch = xfconf_channel_get("xsettings");
        gchar *theme = xfconf_channel_get_string(ch, "/Net/ThemeName", "Adwaita");
        rc->is_dark = (theme && strstr(theme, "dark") != NULL) || (theme && strstr(theme, "Dark") != NULL);
        g_free(theme);
    } else {
        GtkSettings *s = gtk_settings_get_default();
        gchar *name = NULL;
        if (s) g_object_get(s, "gtk-theme-name", &name, NULL);
        rc->is_dark = name && (strstr(name, "dark") || strstr(name, "Dark"));
        g_free(name);
    }
}

static gboolean runcat_update_cpu(gpointer data) {
    RunCatPlugin *rc = data;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return G_SOURCE_CONTINUE;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return G_SOURCE_CONTINUE; }
    fclose(f);
    int n = sscanf(line, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
    if (n < 8) return G_SOURCE_CONTINUE;
    unsigned long long Idle = idle + iowait;
    unsigned long long NonIdle = user + nice + system + irq + softirq + steal;
    unsigned long long Total = Idle + NonIdle;
    if (rc->prev_total != 0) {
        unsigned long long totald = Total - rc->prev_total;
        unsigned long long idled = Idle - rc->prev_idle;
        if (totald > 0) {
            gint cpu = (gint)((totald - idled) * 100 / totald);
            if (cpu < 0) cpu = 0;
            if (cpu > 100) cpu = 100;
            rc->cpu_usage = cpu;
            runcat_update_tooltip(rc);
            if (rc->label) {
                gchar *txt = g_strdup_printf(" %d%%", cpu);
                gtk_label_set_text(GTK_LABEL(rc->label), txt);
                g_free(txt);
            }
            // reschedule only if delay changed to avoid jitter
            gint delay = runcat_get_cpu_delay(rc);
            if (delay != rc->last_delay) {
                if (rc->anim_timeout_id) g_source_remove(rc->anim_timeout_id);
                rc->anim_timeout_id = g_timeout_add(delay, runcat_update_animation, rc);
                rc->last_delay = delay;
            }
        }
    }
    rc->prev_idle = Idle;
    rc->prev_total = Total;
    runcat_update_tooltip(rc);
    return G_SOURCE_CONTINUE;
}

static void runcat_update_gpu(RunCatPlugin *rc) {
    gchar *out = NULL;
    gchar *err = NULL;
    gint status = 0;
    GError *gerr = NULL;
    if (!g_spawn_command_line_sync("nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits",
                                   &out, &err, &status, &gerr)) {
        g_clear_error(&gerr);
        g_free(err);
        g_free(out);
        rc->gpu_usage = -1;
        return;
    }
    g_free(err);
    if (status != 0 || !out || !*out) {
        g_free(out);
        rc->gpu_usage = -1;
        return;
    }
    int usage = 0;
    guint64 used = 0;
    guint64 total = 0;
    if (sscanf(out, "%d, %lu, %lu", &usage, &used, &total) == 3) {
        rc->gpu_usage = usage;
        rc->gpu_mem_used = used;
        rc->gpu_mem_total = total;
    } else {
        rc->gpu_usage = -1;
    }
    g_free(out);
}

static void runcat_update_memory(RunCatPlugin *rc) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { rc->mem_total_mb = 0; rc->mem_avail_mb = 0; return; }
    char line[256];
    gdouble total_kb = 0, avail_kb = 0;
    while (fgets(line, sizeof(line), f)) {
        if (g_str_has_prefix(line, "MemTotal:"))
            sscanf(line, "MemTotal: %lf", &total_kb);
        else if (g_str_has_prefix(line, "MemAvailable:"))
            sscanf(line, "MemAvailable: %lf", &avail_kb);
    }
    fclose(f);
    rc->mem_total_mb = total_kb / 1024.0;
    rc->mem_avail_mb = avail_kb / 1024.0;
}

static void runcat_update_disk(RunCatPlugin *rc) {
    struct statvfs buf;
    if (statvfs("/", &buf) != 0) { rc->disk_pct = 0; return; }
    if (buf.f_blocks == 0) { rc->disk_pct = 0; return; }
    guint64 total = buf.f_blocks;
    guint64 free = buf.f_bavail;
    gint pct = (gint)((total - free) * 100.0 / total);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    rc->disk_pct = pct;
}

static void runcat_update_tooltip(RunCatPlugin *rc) {
    gchar *tip;
    if (rc->gpu_usage >= 0)
        tip = g_strdup_printf("CPU Usage: %d%%\nGPU Usage: %d%% (%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT " MB)\nMemory: %.1f/%.1f GB\nDisk: %d%%",
                              rc->cpu_usage, rc->gpu_usage, rc->gpu_mem_used, rc->gpu_mem_total,
                              (rc->mem_total_mb - rc->mem_avail_mb) / 1024.0, rc->mem_total_mb / 1024.0, rc->disk_pct);
    else
        tip = g_strdup_printf("CPU Usage: %d%%\nGPU Usage: N/A\nMemory: %.1f/%.1f GB\nDisk: %d%%",
                              rc->cpu_usage, (rc->mem_total_mb - rc->mem_avail_mb) / 1024.0,
                              rc->mem_total_mb / 1024.0, rc->disk_pct);
    gtk_widget_set_tooltip_text(rc->ebox, tip);
    g_free(tip);
}

static gboolean runcat_update_stats(gpointer data) {
    RunCatPlugin *rc = data;
    runcat_update_gpu(rc);
    runcat_update_memory(rc);
    runcat_update_disk(rc);
    runcat_update_tooltip(rc);
    return G_SOURCE_CONTINUE;
}

static gboolean runcat_update_animation(gpointer data) {
    RunCatPlugin *rc = data;
    rc->current_frame = (rc->current_frame + 1) % TOTAL_FRAMES;
    gint size = xfce_panel_plugin_get_size(rc->plugin);
    gint icon_size = xfce_panel_plugin_get_icon_size(rc->plugin);
    gint target = (gint)((size > icon_size ? size : icon_size) * 0.75);
    if (target < 16) target = 16;
    if (target > 48) target = 48;
    runcat_ensure_cache(rc, target);
    GdkPixbuf *pix = rc->is_dark ? rc->cache_dark[rc->current_frame] : rc->cache_light[rc->current_frame];
    if (!pix) pix = rc->cache_dark[0] ? rc->cache_dark[0] : rc->cache_light[0];
    if (pix) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(rc->image), pix);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean runcat_size_changed(XfcePanelPlugin *plugin, gint size, gpointer data) {
    RunCatPlugin *rc = data;
    gint icon_size = xfce_panel_plugin_get_icon_size(plugin);
    gint target = (gint)((size > icon_size ? size : icon_size) * 0.75);
    if (target < 16) target = 16;
    if (target > 48) target = 48;
    runcat_ensure_cache(rc, target);
    GdkPixbuf *pix = rc->is_dark ? rc->cache_dark[rc->current_frame] : rc->cache_light[rc->current_frame];
    if (!pix) pix = rc->cache_dark[0] ? rc->cache_dark[0] : rc->cache_light[0];
    if (pix) gtk_image_set_from_pixbuf(GTK_IMAGE(rc->image), pix);
    return TRUE;
}

static void runcat_orientation_changed(XfcePanelPlugin *plugin, GtkOrientation orientation, gpointer data) {
    RunCatPlugin *rc = data;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(rc->box), orientation);
}

static void runcat_free(XfcePanelPlugin *plugin, gpointer data) {
    RunCatPlugin *rc = data;
    if (rc->anim_timeout_id) g_source_remove(rc->anim_timeout_id);
    if (rc->cpu_timeout_id) g_source_remove(rc->cpu_timeout_id);
    if (rc->stats_timeout_id) g_source_remove(rc->stats_timeout_id);
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        if (rc->frames_dark[i]) g_object_unref(rc->frames_dark[i]);
        if (rc->frames_light[i]) g_object_unref(rc->frames_light[i]);
        if (rc->cache_dark[i]) g_object_unref(rc->cache_dark[i]);
        if (rc->cache_light[i]) g_object_unref(rc->cache_light[i]);
    }
    g_free(rc->resource_path);
    g_free(rc);
}

static void runcat_construct(XfcePanelPlugin *plugin) {
    RunCatPlugin *rc = g_new0(RunCatPlugin, 1);
    rc->plugin = plugin;
    rc->current_frame = TOTAL_FRAMES - 1;
    rc->cpu_usage = 0;
    rc->gpu_usage = -1;
    rc->gpu_mem_used = 0;
    rc->gpu_mem_total = 0;
    rc->mem_total_mb = 0;
    rc->mem_avail_mb = 0;
    rc->disk_pct = 0;
    rc->is_dark = FALSE;
    rc->cached_size = -1;
    rc->last_delay = -1;

    runcat_update_theme(rc);
    runcat_load_frames(rc);

    rc->ebox = gtk_event_box_new();
    gtk_widget_show(rc->ebox);
    gtk_container_add(GTK_CONTAINER(plugin), rc->ebox);

    GtkOrientation orient = xfce_panel_plugin_get_orientation(plugin);
    rc->box = gtk_box_new(orient, 2);
    gtk_widget_show(rc->box);
    gtk_container_add(GTK_CONTAINER(rc->ebox), rc->box);

    rc->image = gtk_image_new();
    gtk_widget_show(rc->image);
    gtk_box_pack_start(GTK_BOX(rc->box), rc->image, FALSE, FALSE, 0);

    rc->label = gtk_label_new(" 0%");
    gtk_widget_show(rc->label);
    gtk_box_pack_start(GTK_BOX(rc->box), rc->label, FALSE, FALSE, 0);

    gtk_widget_set_tooltip_text(rc->ebox, "Loading...");

    // initial frame: show 0 without extra increment
    rc->current_frame = 0;
    {
        gint size = xfce_panel_plugin_get_size(plugin);
        gint icon_size = xfce_panel_plugin_get_icon_size(plugin);
        gint target = icon_size > 0 ? icon_size : (size > 0 ? size : 24);
        if (target < 16) target = 16;
        if (target > 48) target = 48;
        runcat_ensure_cache(rc, target);
        GdkPixbuf *pix = rc->is_dark ? rc->cache_dark[0] : rc->cache_light[0];
        if (pix) gtk_image_set_from_pixbuf(GTK_IMAGE(rc->image), pix);
    }

    // timers: animation 50-100ms, cpu poll 1s, stats poll 2s
    gint delay = runcat_get_cpu_delay(rc);
    rc->last_delay = delay;
    rc->anim_timeout_id = g_timeout_add(delay, runcat_update_animation, rc);
    rc->cpu_timeout_id = g_timeout_add(1000, runcat_update_cpu, rc);
    rc->stats_timeout_id = g_timeout_add(2000, runcat_update_stats, rc);
    // poll immediately for initial cpu and stats
    runcat_update_cpu(rc);
    runcat_update_stats(rc);

    g_signal_connect(plugin, "size-changed", G_CALLBACK(runcat_size_changed), rc);
    g_signal_connect(plugin, "orientation-changed", G_CALLBACK(runcat_orientation_changed), rc);
    g_signal_connect(plugin, "free-data", G_CALLBACK(runcat_free), rc);

    xfce_panel_plugin_menu_show_about(plugin);
}

XFCE_PANEL_PLUGIN_REGISTER(runcat_construct)
