/*
 * Copyright (c) 2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef TRAYCON_H
#define TRAYCON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct traycon traycon;

/* Called when the tray icon is left-clicked. */
typedef void (*traycon_click_cb)(traycon *tray, void *userdata);

/*
 * Linux backend selection (no-op on macOS / Windows).
 *
 * TRAYCON_BACKEND_AUTO – try SNI (D-Bus), fall back to X11  (default)
 * TRAYCON_BACKEND_SNI  – StatusNotifierItem via D-Bus (Wayland / KDE)
 * TRAYCON_BACKEND_X11  – XEmbed system tray protocol  (X11)
 *
 * Call traycon_set_preferred_backend() before traycon_create().
 */
#define TRAYCON_BACKEND_AUTO  0
#define TRAYCON_BACKEND_SNI   1
#define TRAYCON_BACKEND_X11   2

void traycon_set_preferred_backend(int backend);

/*
 * Create a tray icon from raw RGBA pixel data.
 *
 * rgba     - pointer to width * height * 4 bytes (R, G, B, A per pixel,
 *            row-major, top-left origin)
 * width    - icon width in pixels  (recommended: 32 or 48)
 * height   - icon height in pixels
 * cb       - called on left-click (may be NULL)
 * userdata - forwarded to cb
 *
 * Returns NULL on failure.
 */
traycon *traycon_create(const unsigned char *rgba, int width, int height,
                        traycon_click_cb cb, void *userdata);

/*
 * Replace the icon image.  Same format as traycon_create().
 * Returns 0 on success, -1 on failure.
 */
int traycon_update_icon(traycon *tray, const unsigned char *rgba,
                        int width, int height);

/*
 * Process pending events (non-blocking).
 * Call this regularly from your main loop.
 * Returns 0 normally, -1 if the tray is no longer functional.
 */
int traycon_step(traycon *tray);

/*
 * Remove the tray icon and free all resources.
 * Passing NULL is safe.
 */
void traycon_destroy(traycon *tray);

/*
 * Show or hide the tray icon.
 * visible: non-zero to show, zero to hide.
 * Returns 0 on success, -1 on failure.
 */
int traycon_set_visible(traycon *tray, int visible);

#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------------------- */
/*  IMPLEMENTATION                                                        */
/*                                                                        */
/*  In exactly ONE C file, define TRAYCON_IMPLEMENTATION before including */
/*  this header:                                                          */
/*                                                                        */
/*      #define TRAYCON_IMPLEMENTATION                                    */
/*      #include "traycon.h"                                              */
/* ---------------------------------------------------------------------- */

#ifdef TRAYCON_IMPLEMENTATION
#ifndef TRAYCON_IMPLEMENTATION_GUARD
#define TRAYCON_IMPLEMENTATION_GUARD

/* ====== begin traycon_linux.c ====== */
#ifdef __linux__
/*
 * traycon – Linux implementation
 *
 * Supports two backends:
 *   1. SNI  – StatusNotifierItem via D-Bus (Wayland / KDE / modern DEs)
 *   2. X11  – XEmbed system tray protocol  (X11 / traditional DEs)
 *
 * By default auto-detects: tries SNI first, falls back to X11.
 * Override with traycon_set_preferred_backend() or compile-time defines
 * TRAYCON_NO_SNI / TRAYCON_NO_X11.
 *
 * Dependencies:
 *   SNI  – libdbus-1  (pkg-config dbus-1)
 *   X11  – libX11     (pkg-config x11)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TRAYCON_NO_SNI
#include <dbus/dbus.h>
#endif

#ifndef TRAYCON_NO_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

/* ------------------------------------------------------------------ */
/*  Backend preference                                                 */
/* ------------------------------------------------------------------ */

static int traycon__preferred_backend = 0; /* TRAYCON_BACKEND_AUTO */

void traycon_set_preferred_backend(int backend)
{
    traycon__preferred_backend = backend;
}

/* ------------------------------------------------------------------ */
/*  struct traycon                                                     */
/* ------------------------------------------------------------------ */

struct traycon {
    /* vtable -------------------------------------------------------- */
    int  (*fn_update_icon)(traycon *, const unsigned char *, int, int);
    int  (*fn_step)(traycon *);
    void (*fn_destroy)(traycon *);
    int  (*fn_set_visible)(traycon *, int);

    /* common -------------------------------------------------------- */
    traycon_click_cb cb;
    void            *userdata;
    int              visible;

    /* backend-specific (only one active at a time) ------------------ */
    union {
#ifndef TRAYCON_NO_SNI
        struct {
            DBusConnection  *conn;
            unsigned char   *icon_argb;   /* ARGB32 network-byte-order */
            int              icon_w;
            int              icon_h;
            int              icon_len;    /* icon_w * icon_h * 4       */
            char             bus_name[128];
        } sni;
#endif
#ifndef TRAYCON_NO_X11
        struct {
            Display         *dpy;
            Window           win;
            GC               gc;
            Visual          *visual;
            Colormap         cmap;
            int              depth;
            int              screen;
            int              own_cmap;    /* did we create the cmap?   */

            unsigned char   *icon_rgba;   /* copy of original RGBA     */
            int              icon_w;
            int              icon_h;

            XImage          *ximg;        /* converted for blitting    */
            int              win_w;
            int              win_h;

            /* cached atoms */
            Atom             xa_tray_sel;
            Atom             xa_tray_opcode;
            Atom             xa_tray_visual;
            Atom             xa_xembed_info;
            Atom             xa_net_wm_icon;
            Atom             xa_manager;
        } x11;
#endif
        char _pad; /* ensure the union is never empty */
    };
};

/* ================================================================== */
/*                                                                      */
/*  SNI BACKEND                                                         */
/*                                                                      */
/* ================================================================== */

#ifndef TRAYCON_NO_SNI

static int sni_id_counter = 0;

/* ------------------------------------------------------------------ */
/*  RGBA → ARGB-network-byte-order conversion                         */
/* ------------------------------------------------------------------ */

static unsigned char *sni_rgba_to_argb_nbo(const unsigned char *rgba,
                                           int w, int h)
{
    int n = w * h;
    unsigned char *out = (unsigned char *)malloc((size_t)n * 4);
    if (!out) return NULL;
    for (int i = 0; i < n; i++) {
        out[i * 4 + 0] = rgba[i * 4 + 3]; /* A */
        out[i * 4 + 1] = rgba[i * 4 + 0]; /* R */
        out[i * 4 + 2] = rgba[i * 4 + 1]; /* G */
        out[i * 4 + 3] = rgba[i * 4 + 2]; /* B */
    }
    return out;
}

/* ------------------------------------------------------------------ */
/*  Introspection XML                                                  */
/* ------------------------------------------------------------------ */

static const char SNI_INTROSPECT_XML[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object "
    "Introspection 1.0//EN\"\n"
    "  \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.kde.StatusNotifierItem\">\n"
    "    <method name=\"Activate\">\n"
    "      <arg name=\"x\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"y\" type=\"i\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"SecondaryActivate\">\n"
    "      <arg name=\"x\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"y\" type=\"i\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"ContextMenu\">\n"
    "      <arg name=\"x\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"y\" type=\"i\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"Scroll\">\n"
    "      <arg name=\"delta\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"orientation\" type=\"s\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"NewIcon\"/>\n"
    "    <signal name=\"NewTitle\"/>\n"
    "    <signal name=\"NewStatus\">\n"
    "      <arg type=\"s\"/>\n"
    "    </signal>\n"
    "    <property name=\"Category\"           type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"Id\"                 type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"Title\"              type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"Status\"             type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"WindowId\"           type=\"u\"          access=\"read\"/>\n"
    "    <property name=\"ItemIsMenu\"         type=\"b\"          access=\"read\"/>\n"
    "    <property name=\"IconName\"           type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"IconPixmap\"         type=\"a(iiay)\"    access=\"read\"/>\n"
    "    <property name=\"OverlayIconName\"    type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"OverlayIconPixmap\"  type=\"a(iiay)\"    access=\"read\"/>\n"
    "    <property name=\"AttentionIconName\"  type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"AttentionIconPixmap\" type=\"a(iiay)\"   access=\"read\"/>\n"
    "    <property name=\"AttentionMovieName\" type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"ToolTip\"            type=\"(sa(iiay)ss)\" access=\"read\"/>\n"
    "    <property name=\"IconThemePath\"      type=\"s\"          access=\"read\"/>\n"
    "    <property name=\"Menu\"               type=\"o\"          access=\"read\"/>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
    "    <method name=\"Get\">\n"
    "      <arg name=\"interface\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"property\"  type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"value\"     type=\"v\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetAll\">\n"
    "      <arg name=\"interface\"  type=\"s\"    direction=\"in\"/>\n"
    "      <arg name=\"properties\" type=\"a{sv}\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

/* ------------------------------------------------------------------ */
/*  D-Bus variant helpers                                              */
/* ------------------------------------------------------------------ */

static void sni_var_string(DBusMessageIter *parent, const char *val)
{
    DBusMessageIter v;
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT, "s", &v);
    dbus_message_iter_append_basic(&v, DBUS_TYPE_STRING, &val);
    dbus_message_iter_close_container(parent, &v);
}

static void sni_var_uint32(DBusMessageIter *parent, dbus_uint32_t val)
{
    DBusMessageIter v;
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT, "u", &v);
    dbus_message_iter_append_basic(&v, DBUS_TYPE_UINT32, &val);
    dbus_message_iter_close_container(parent, &v);
}

static void sni_var_bool(DBusMessageIter *parent, dbus_bool_t val)
{
    DBusMessageIter v;
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT, "b", &v);
    dbus_message_iter_append_basic(&v, DBUS_TYPE_BOOLEAN, &val);
    dbus_message_iter_close_container(parent, &v);
}

static void sni_var_objectpath(DBusMessageIter *parent, const char *val)
{
    DBusMessageIter v;
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT, "o", &v);
    dbus_message_iter_append_basic(&v, DBUS_TYPE_OBJECT_PATH, &val);
    dbus_message_iter_close_container(parent, &v);
}

/* Write variant a(iiay).  If data is NULL an empty array is written. */
static void sni_var_icon_pixmap(DBusMessageIter *parent,
                                const unsigned char *argb, int w, int h)
{
    DBusMessageIter v, arr;
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT,
                                     "a(iiay)", &v);
    dbus_message_iter_open_container(&v, DBUS_TYPE_ARRAY,
                                     "(iiay)", &arr);
    if (argb && w > 0 && h > 0) {
        DBusMessageIter st, da;
        int len = w * h * 4;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, NULL, &st);
        dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &w);
        dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &h);
        dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "y", &da);
        dbus_message_iter_append_fixed_array(&da, DBUS_TYPE_BYTE, &argb, len);
        dbus_message_iter_close_container(&st, &da);
        dbus_message_iter_close_container(&arr, &st);
    }
    dbus_message_iter_close_container(&v, &arr);
    dbus_message_iter_close_container(parent, &v);
}

/* Write variant (sa(iiay)ss) — empty tooltip. */
static void sni_var_tooltip(DBusMessageIter *parent)
{
    DBusMessageIter v, st, arr;
    const char *empty = "";
    dbus_message_iter_open_container(parent, DBUS_TYPE_VARIANT,
                                     "(sa(iiay)ss)", &v);
    dbus_message_iter_open_container(&v, DBUS_TYPE_STRUCT, NULL, &st);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &empty);
    dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "(iiay)", &arr);
    dbus_message_iter_close_container(&st, &arr);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &empty);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &empty);
    dbus_message_iter_close_container(&v, &st);
    dbus_message_iter_close_container(parent, &v);
}

/* ------------------------------------------------------------------ */
/*  Property dispatch                                                  */
/* ------------------------------------------------------------------ */

static int sni_append_property(DBusMessageIter *iter, const char *prop,
                               traycon *tray)
{
    if      (!strcmp(prop, "Category"))            sni_var_string(iter, "ApplicationStatus");
    else if (!strcmp(prop, "Id"))                  sni_var_string(iter, "traycon");
    else if (!strcmp(prop, "Title"))               sni_var_string(iter, "traycon");
    else if (!strcmp(prop, "Status"))              sni_var_string(iter, tray->visible ? "Active" : "Passive");
    else if (!strcmp(prop, "WindowId"))            sni_var_uint32(iter, 0);
    else if (!strcmp(prop, "IconThemePath"))       sni_var_string(iter, "");
    else if (!strcmp(prop, "IconName"))            sni_var_string(iter, "");
    else if (!strcmp(prop, "IconPixmap"))          sni_var_icon_pixmap(iter, tray->sni.icon_argb,
                                                                       tray->sni.icon_w,
                                                                       tray->sni.icon_h);
    else if (!strcmp(prop, "OverlayIconName"))     sni_var_string(iter, "");
    else if (!strcmp(prop, "OverlayIconPixmap"))   sni_var_icon_pixmap(iter, NULL, 0, 0);
    else if (!strcmp(prop, "AttentionIconName"))   sni_var_string(iter, "");
    else if (!strcmp(prop, "AttentionIconPixmap")) sni_var_icon_pixmap(iter, NULL, 0, 0);
    else if (!strcmp(prop, "AttentionMovieName"))  sni_var_string(iter, "");
    else if (!strcmp(prop, "ToolTip"))             sni_var_tooltip(iter);
    else if (!strcmp(prop, "ItemIsMenu"))          sni_var_bool(iter, FALSE);
    else if (!strcmp(prop, "Menu"))                sni_var_objectpath(iter, "/NO_DBUSMENU");
    else return -1;
    return 0;
}

static const char *SNI_ALL_PROPS[] = {
    "Category", "Id", "Title", "Status", "WindowId",
    "IconThemePath", "IconName", "IconPixmap",
    "OverlayIconName", "OverlayIconPixmap",
    "AttentionIconName", "AttentionIconPixmap", "AttentionMovieName",
    "ToolTip", "ItemIsMenu", "Menu",
    NULL
};

/* ------------------------------------------------------------------ */
/*  D-Bus message handler for /StatusNotifierItem                      */
/* ------------------------------------------------------------------ */

static DBusHandlerResult
sni_handle_message(DBusConnection *conn, DBusMessage *msg, void *data)
{
    traycon *tray = (traycon *)data;

    /* --- Introspect ------------------------------------------------ */
    if (dbus_message_is_method_call(msg,
            "org.freedesktop.DBus.Introspectable", "Introspect")) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        const char *xml = SNI_INTROSPECT_XML;
        dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    /* --- Properties.Get -------------------------------------------- */
    if (dbus_message_is_method_call(msg,
            "org.freedesktop.DBus.Properties", "Get")) {
        const char *iface = NULL, *prop = NULL;
        if (!dbus_message_get_args(msg, NULL,
                DBUS_TYPE_STRING, &iface,
                DBUS_TYPE_STRING, &prop,
                DBUS_TYPE_INVALID))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        if (sni_append_property(&iter, prop, tray) < 0) {
            dbus_message_unref(reply);
            reply = dbus_message_new_error_printf(msg,
                DBUS_ERROR_UNKNOWN_PROPERTY,
                "Unknown property: %s", prop);
        }
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    /* --- Properties.GetAll ----------------------------------------- */
    if (dbus_message_is_method_call(msg,
            "org.freedesktop.DBus.Properties", "GetAll")) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        DBusMessageIter iter, dict;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                         "{sv}", &dict);
        for (int i = 0; SNI_ALL_PROPS[i]; i++) {
            DBusMessageIter entry;
            dbus_message_iter_open_container(&dict,
                DBUS_TYPE_DICT_ENTRY, NULL, &entry);
            dbus_message_iter_append_basic(&entry,
                DBUS_TYPE_STRING, &SNI_ALL_PROPS[i]);
            sni_append_property(&entry, SNI_ALL_PROPS[i], tray);
            dbus_message_iter_close_container(&dict, &entry);
        }
        dbus_message_iter_close_container(&iter, &dict);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    /* --- Activate (left click) ------------------------------------- */
    if (dbus_message_is_method_call(msg,
            "org.kde.StatusNotifierItem", "Activate") ||
        dbus_message_is_method_call(msg,
            "org.freedesktop.StatusNotifierItem", "Activate")) {
        if (tray->cb) tray->cb(tray, tray->userdata);
        DBusMessage *reply = dbus_message_new_method_return(msg);
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    /* --- Other SNI methods (no-ops) -------------------------------- */
    {
        const char *iface = dbus_message_get_interface(msg);
        if (iface &&
            (!strcmp(iface, "org.kde.StatusNotifierItem") ||
             !strcmp(iface, "org.freedesktop.StatusNotifierItem"))) {
            DBusMessage *reply = dbus_message_new_method_return(msg);
            dbus_connection_send(conn, reply, NULL);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ------------------------------------------------------------------ */
/*  SNI backend functions                                              */
/* ------------------------------------------------------------------ */

static int  sni_update_icon(traycon *tray, const unsigned char *rgba,
                            int width, int height);
static int  sni_step(traycon *tray);
static void sni_destroy(traycon *tray);
static int  sni_set_visible(traycon *tray, int visible);

static traycon *sni_try_create(const unsigned char *rgba, int width,
                               int height, traycon_click_cb cb,
                               void *userdata)
{
    traycon *tray = (traycon *)calloc(1, sizeof *tray);
    if (!tray) return NULL;

    tray->fn_update_icon = sni_update_icon;
    tray->fn_step        = sni_step;
    tray->fn_destroy     = sni_destroy;
    tray->fn_set_visible = sni_set_visible;
    tray->cb             = cb;
    tray->userdata       = userdata;
    tray->visible        = 1;

    /* Convert icon -------------------------------------------------- */
    tray->sni.icon_argb = sni_rgba_to_argb_nbo(rgba, width, height);
    if (!tray->sni.icon_argb) { free(tray); return NULL; }
    tray->sni.icon_w   = width;
    tray->sni.icon_h   = height;
    tray->sni.icon_len = width * height * 4;

    /* Connect to the session bus ------------------------------------ */
    DBusError err;
    dbus_error_init(&err);
    tray->sni.conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "traycon: D-Bus connection failed: %s\n", err.message);
        dbus_error_free(&err);
        free(tray->sni.icon_argb);
        free(tray);
        return NULL;
    }

    /* Request a well-known name ------------------------------------- */
    int id = ++sni_id_counter;
    snprintf(tray->sni.bus_name, sizeof tray->sni.bus_name,
             "org.kde.StatusNotifierItem-%d-%d", (int)getpid(), id);

    int ret = dbus_bus_request_name(tray->sni.conn, tray->sni.bus_name,
                                    DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err) ||
        ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "traycon: could not acquire bus name %s: %s\n",
                tray->sni.bus_name,
                dbus_error_is_set(&err) ? err.message : "name taken");
        dbus_error_free(&err);
        free(tray->sni.icon_argb);
        free(tray);
        return NULL;
    }

    /* Register object path ------------------------------------------ */
    static const DBusObjectPathVTable vtable = {
        .unregister_function = NULL,
        .message_function    = sni_handle_message,
    };
    if (!dbus_connection_register_object_path(tray->sni.conn,
            "/StatusNotifierItem", &vtable, tray)) {
        fprintf(stderr, "traycon: failed to register object path\n");
        free(tray->sni.icon_argb);
        free(tray);
        return NULL;
    }

    /* Register with StatusNotifierWatcher --------------------------- */
    DBusMessage *reg = dbus_message_new_method_call(
        "org.kde.StatusNotifierWatcher",
        "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher",
        "RegisterStatusNotifierItem");
    const char *svc = tray->sni.bus_name;
    dbus_message_append_args(reg,
        DBUS_TYPE_STRING, &svc, DBUS_TYPE_INVALID);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        tray->sni.conn, reg, 5000, &err);
    dbus_message_unref(reg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "traycon: watcher registration failed: %s\n",
                err.message);
        dbus_error_free(&err);
        /* Treat as fatal for auto-detect – caller can try X11 next. */
        dbus_connection_unregister_object_path(tray->sni.conn,
                                               "/StatusNotifierItem");
        DBusError rel_err;
        dbus_error_init(&rel_err);
        dbus_bus_release_name(tray->sni.conn, tray->sni.bus_name, &rel_err);
        dbus_error_free(&rel_err);
        dbus_connection_flush(tray->sni.conn);
        dbus_connection_unref(tray->sni.conn);
        free(tray->sni.icon_argb);
        free(tray);
        return NULL;
    }
    if (reply) dbus_message_unref(reply);

    return tray;
}

static int sni_update_icon(traycon *tray, const unsigned char *rgba,
                           int width, int height)
{
    unsigned char *new_argb = sni_rgba_to_argb_nbo(rgba, width, height);
    if (!new_argb) return -1;

    free(tray->sni.icon_argb);
    tray->sni.icon_argb = new_argb;
    tray->sni.icon_w    = width;
    tray->sni.icon_h    = height;
    tray->sni.icon_len  = width * height * 4;

    /* Tell the tray host to re-read the icon */
    DBusMessage *sig = dbus_message_new_signal(
        "/StatusNotifierItem",
        "org.kde.StatusNotifierItem",
        "NewIcon");
    if (sig) {
        dbus_connection_send(tray->sni.conn, sig, NULL);
        dbus_message_unref(sig);
        dbus_connection_flush(tray->sni.conn);
    }
    return 0;
}

static int sni_step(traycon *tray)
{
    if (!tray->sni.conn) return -1;

    /* Non-blocking read/write */
    if (!dbus_connection_read_write(tray->sni.conn, 0))
        return -1;                    /* disconnected */

    while (dbus_connection_dispatch(tray->sni.conn) ==
           DBUS_DISPATCH_DATA_REMAINS)
        ;

    return 0;
}

static void sni_destroy(traycon *tray)
{
    if (tray->sni.conn) {
        dbus_connection_unregister_object_path(tray->sni.conn,
                                               "/StatusNotifierItem");
        DBusError err;
        dbus_error_init(&err);
        dbus_bus_release_name(tray->sni.conn, tray->sni.bus_name, &err);
        dbus_error_free(&err);
        dbus_connection_flush(tray->sni.conn);
        dbus_connection_unref(tray->sni.conn);
    }
    free(tray->sni.icon_argb);
}

static int sni_set_visible(traycon *tray, int visible)
{
    tray->visible = visible;

    const char *status = visible ? "Active" : "Passive";
    DBusMessage *sig = dbus_message_new_signal(
        "/StatusNotifierItem",
        "org.kde.StatusNotifierItem",
        "NewStatus");
    if (sig) {
        dbus_message_append_args(sig,
            DBUS_TYPE_STRING, &status, DBUS_TYPE_INVALID);
        dbus_connection_send(tray->sni.conn, sig, NULL);
        dbus_message_unref(sig);
        dbus_connection_flush(tray->sni.conn);
    }
    return 0;
}

#endif /* !TRAYCON_NO_SNI */

/* ================================================================== */
/*                                                                      */
/*  X11 BACKEND (XEmbed system tray protocol)                           */
/*                                                                      */
/* ================================================================== */

#ifndef TRAYCON_NO_X11

#define X11_SYSTEM_TRAY_REQUEST_DOCK    0
#define X11_SYSTEM_TRAY_BEGIN_MESSAGE   1
#define X11_SYSTEM_TRAY_CANCEL_MESSAGE  2
#define X11_XEMBED_MAPPED              (1 << 0)

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int x11_mask_shift(unsigned long mask)
{
    int s = 0;
    if (!mask) return 0;
    while (!(mask & 1)) { mask >>= 1; s++; }
    return s;
}

/*
 * Build an XImage of size (dw × dh) from RGBA source of size (sw × sh)
 * using nearest-neighbour scaling.  Handles both depth 24 and 32.
 */
static XImage *x11_make_ximage(Display *dpy, Visual *vis, int depth,
                                const unsigned char *rgba,
                                int sw, int sh, int dw, int dh)
{
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return NULL;

    int bpp = 4; /* works for depth >= 24 */
    char *pixels = (char *)calloc((size_t)dw * dh, bpp);
    if (!pixels) return NULL;

    XImage *img = XCreateImage(dpy, vis, depth, ZPixmap, 0,
                                pixels, dw, dh, 32, dw * bpp);
    if (!img) { free(pixels); return NULL; }

    int rs = x11_mask_shift(vis->red_mask);
    int gs = x11_mask_shift(vis->green_mask);
    int bs = x11_mask_shift(vis->blue_mask);

    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            int i  = (sy * sw + sx) * 4;

            unsigned r = rgba[i + 0];
            unsigned g = rgba[i + 1];
            unsigned b = rgba[i + 2];
            unsigned a = rgba[i + 3];

            if (depth == 32) {
                /* Pre-multiply alpha (compositors expect this). */
                r = (r * a + 127) / 255;
                g = (g * a + 127) / 255;
                b = (b * a + 127) / 255;
            }

            unsigned long pixel =
                ((unsigned long)(r << rs) & vis->red_mask)   |
                ((unsigned long)(g << gs) & vis->green_mask)  |
                ((unsigned long)(b << bs) & vis->blue_mask);

            if (depth == 32)
                pixel |= ((unsigned long)a << 24);

            XPutPixel(img, x, y, pixel);
        }
    }

    return img;
}

/*
 * Set the _NET_WM_ICON property (ARGB, native byte order).
 */
static void x11_set_net_wm_icon(Display *dpy, Window win, Atom xa,
                                 const unsigned char *rgba, int w, int h)
{
    int n = w * h;
    /* _NET_WM_ICON data: [width, height, pixel, pixel, ...] */
    unsigned long *data = (unsigned long *)malloc(((size_t)n + 2) *
                                                   sizeof(unsigned long));
    if (!data) return;
    data[0] = (unsigned long)w;
    data[1] = (unsigned long)h;
    for (int i = 0; i < n; i++) {
        unsigned long a = rgba[i * 4 + 3];
        unsigned long r = rgba[i * 4 + 0];
        unsigned long g = rgba[i * 4 + 1];
        unsigned long b = rgba[i * 4 + 2];
        data[2 + i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    XChangeProperty(dpy, win, xa, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)data, n + 2);
    free(data);
}

/*
 * Rebuild the XImage after a window resize or icon change.
 */
static void x11_rebuild_ximage(traycon *tray)
{
    if (tray->x11.ximg) {
        XDestroyImage(tray->x11.ximg);   /* frees ximg->data too */
        tray->x11.ximg = NULL;
    }

    if (!tray->x11.icon_rgba || tray->x11.icon_w <= 0 ||
        tray->x11.icon_h <= 0 || tray->x11.win_w <= 0 ||
        tray->x11.win_h <= 0)
        return;

    tray->x11.ximg = x11_make_ximage(
        tray->x11.dpy, tray->x11.visual, tray->x11.depth,
        tray->x11.icon_rgba,
        tray->x11.icon_w, tray->x11.icon_h,
        tray->x11.win_w,  tray->x11.win_h);
}

/*
 * Draw the current icon into the X11 window.
 */
static void x11_draw(traycon *tray)
{
    if (!tray->x11.ximg) return;
    XPutImage(tray->x11.dpy, tray->x11.win, tray->x11.gc,
              tray->x11.ximg, 0, 0, 0, 0,
              (unsigned)tray->x11.win_w, (unsigned)tray->x11.win_h);
}

/* ------------------------------------------------------------------ */
/*  X11 backend functions                                              */
/* ------------------------------------------------------------------ */

static int  x11_update_icon(traycon *tray, const unsigned char *rgba,
                            int width, int height);
static int  x11_step(traycon *tray);
static void x11_destroy(traycon *tray);
static int  x11_set_visible(traycon *tray, int visible);

static traycon *x11_try_create(const unsigned char *rgba, int width,
                               int height, traycon_click_cb cb,
                               void *userdata)
{
    /* Check if X11 is available */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return NULL;

    int screen = DefaultScreen(dpy);

    /* Intern atoms -------------------------------------------------- */
    char tray_sel_name[64];
    snprintf(tray_sel_name, sizeof tray_sel_name,
             "_NET_SYSTEM_TRAY_S%d", screen);

    Atom xa_tray_sel     = XInternAtom(dpy, tray_sel_name, False);
    Atom xa_tray_opcode  = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    Atom xa_tray_visual  = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);
    Atom xa_xembed_info  = XInternAtom(dpy, "_XEMBED_INFO", False);
    Atom xa_net_wm_icon  = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom xa_manager      = XInternAtom(dpy, "MANAGER", False);

    /* Find the system tray manager ---------------------------------- */
    Window tray_mgr = XGetSelectionOwner(dpy, xa_tray_sel);
    if (tray_mgr == None) {
        fprintf(stderr, "traycon: no X11 system tray found\n");
        XCloseDisplay(dpy);
        return NULL;
    }

    /* Determine the visual the tray wants us to use ----------------- */
    Visual *vis     = DefaultVisual(dpy, screen);
    int     depth   = DefaultDepth(dpy, screen);
    Colormap cmap   = DefaultColormap(dpy, screen);
    int own_cmap    = 0;

    {
        Atom actual_type;
        int  actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop_data = NULL;

        if (XGetWindowProperty(dpy, tray_mgr, xa_tray_visual,
                               0, 1, False, XA_VISUALID,
                               &actual_type, &actual_format,
                               &nitems, &bytes_after,
                               &prop_data) == Success &&
            nitems > 0 && prop_data)
        {
            VisualID vid = *(VisualID *)prop_data;
            XVisualInfo tmpl;
            tmpl.visualid = vid;
            int nvis = 0;
            XVisualInfo *vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &nvis);
            if (vi && nvis > 0) {
                vis   = vi[0].visual;
                depth = vi[0].depth;
                cmap  = XCreateColormap(dpy, RootWindow(dpy, screen),
                                        vis, AllocNone);
                own_cmap = 1;
            }
            if (vi) XFree(vi);
        }
        if (prop_data) XFree(prop_data);
    }

    /* Create the icon window ---------------------------------------- */
    XSetWindowAttributes attr;
    memset(&attr, 0, sizeof attr);
    attr.colormap         = cmap;
    attr.background_pixel = 0;
    attr.border_pixel     = 0;
    unsigned long amask   = CWColormap | CWBackPixel | CWBorderPixel;

    Window win = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        0, 0, (unsigned)width, (unsigned)height, 0,
        depth, InputOutput, vis,
        amask, &attr);
    if (!win) {
        fprintf(stderr, "traycon: XCreateWindow failed\n");
        if (own_cmap) XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        return NULL;
    }

    /* Select events we care about */
    XSelectInput(dpy, win,
                 ExposureMask | ButtonPressMask | StructureNotifyMask);

    /* Set _XEMBED_INFO: version 0, flags = XEMBED_MAPPED */
    unsigned long xembed_data[2] = { 0, X11_XEMBED_MAPPED };
    XChangeProperty(dpy, win, xa_xembed_info, xa_xembed_info,
                    32, PropModeReplace,
                    (unsigned char *)xembed_data, 2);

    /* Set _NET_WM_ICON */
    x11_set_net_wm_icon(dpy, win, xa_net_wm_icon, rgba, width, height);

    /* Map the window before docking (some tray managers require this) */
    XMapRaised(dpy, win);

    /* Send SYSTEM_TRAY_REQUEST_DOCK to the tray manager ------------- */
    {
        XEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.xclient.type         = ClientMessage;
        ev.xclient.window       = tray_mgr;
        ev.xclient.message_type = xa_tray_opcode;
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = CurrentTime;
        ev.xclient.data.l[1]    = X11_SYSTEM_TRAY_REQUEST_DOCK;
        ev.xclient.data.l[2]    = (long)win;

        XSendEvent(dpy, tray_mgr, False, NoEventMask, &ev);
        XFlush(dpy);
    }

    /* Create the GC ------------------------------------------------- */
    GC gc = XCreateGC(dpy, win, 0, NULL);

    /* Allocate traycon ---------------------------------------------- */
    traycon *tray = (traycon *)calloc(1, sizeof *tray);
    if (!tray) {
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        if (own_cmap) XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        return NULL;
    }

    tray->fn_update_icon = x11_update_icon;
    tray->fn_step        = x11_step;
    tray->fn_destroy     = x11_destroy;
    tray->fn_set_visible = x11_set_visible;
    tray->cb             = cb;
    tray->userdata       = userdata;
    tray->visible        = 1;

    tray->x11.dpy            = dpy;
    tray->x11.win            = win;
    tray->x11.gc             = gc;
    tray->x11.visual         = vis;
    tray->x11.cmap           = cmap;
    tray->x11.depth          = depth;
    tray->x11.screen         = screen;
    tray->x11.own_cmap       = own_cmap;
    tray->x11.win_w          = width;
    tray->x11.win_h          = height;
    tray->x11.xa_tray_sel    = xa_tray_sel;
    tray->x11.xa_tray_opcode = xa_tray_opcode;
    tray->x11.xa_tray_visual = xa_tray_visual;
    tray->x11.xa_xembed_info = xa_xembed_info;
    tray->x11.xa_net_wm_icon = xa_net_wm_icon;
    tray->x11.xa_manager     = xa_manager;

    /* Copy the RGBA data */
    size_t rgba_size = (size_t)width * height * 4;
    tray->x11.icon_rgba = (unsigned char *)malloc(rgba_size);
    if (!tray->x11.icon_rgba) {
        XFreeGC(dpy, gc);
        XDestroyWindow(dpy, win);
        if (own_cmap) XFreeColormap(dpy, cmap);
        XCloseDisplay(dpy);
        free(tray);
        return NULL;
    }
    memcpy(tray->x11.icon_rgba, rgba, rgba_size);
    tray->x11.icon_w = width;
    tray->x11.icon_h = height;

    /* Build the initial XImage */
    x11_rebuild_ximage(tray);

    return tray;
}

static int x11_update_icon(traycon *tray, const unsigned char *rgba,
                           int width, int height)
{
    /* Replace stored RGBA data */
    size_t rgba_size = (size_t)width * height * 4;
    unsigned char *new_rgba = (unsigned char *)malloc(rgba_size);
    if (!new_rgba) return -1;
    memcpy(new_rgba, rgba, rgba_size);

    free(tray->x11.icon_rgba);
    tray->x11.icon_rgba = new_rgba;
    tray->x11.icon_w    = width;
    tray->x11.icon_h    = height;

    /* Update _NET_WM_ICON */
    x11_set_net_wm_icon(tray->x11.dpy, tray->x11.win,
                         tray->x11.xa_net_wm_icon, rgba, width, height);

    /* Rebuild XImage at current window size and redraw */
    x11_rebuild_ximage(tray);
    x11_draw(tray);
    XFlush(tray->x11.dpy);
    return 0;
}

static int x11_step(traycon *tray)
{
    Display *dpy = tray->x11.dpy;
    if (!dpy) return -1;

    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0)
                x11_draw(tray);
            break;

        case ButtonPress:
            if (ev.xbutton.button == Button1) {
                if (tray->cb) tray->cb(tray, tray->userdata);
            }
            break;

        case ConfigureNotify:
            if (ev.xconfigure.width  != tray->x11.win_w ||
                ev.xconfigure.height != tray->x11.win_h)
            {
                tray->x11.win_w = ev.xconfigure.width;
                tray->x11.win_h = ev.xconfigure.height;
                x11_rebuild_ximage(tray);
                x11_draw(tray);
            }
            break;

        case DestroyNotify:
            if (ev.xdestroywindow.window == tray->x11.win) {
                tray->x11.win = None;
                return -1;
            }
            break;

        case ReparentNotify:
            /* Normal: the tray manager reparents our window. */
            break;

        default:
            break;
        }
    }
    return 0;
}

static void x11_destroy(traycon *tray)
{
    Display *dpy = tray->x11.dpy;
    if (!dpy) return;

    if (tray->x11.ximg) {
        XDestroyImage(tray->x11.ximg);
        tray->x11.ximg = NULL;
    }
    free(tray->x11.icon_rgba);
    tray->x11.icon_rgba = NULL;

    if (tray->x11.gc) XFreeGC(dpy, tray->x11.gc);
    if (tray->x11.win) XDestroyWindow(dpy, tray->x11.win);
    if (tray->x11.own_cmap) XFreeColormap(dpy, tray->x11.cmap);
    XCloseDisplay(dpy);
    tray->x11.dpy = NULL;
}

static int x11_set_visible(traycon *tray, int visible)
{
    tray->visible = visible;

    /* Update the XEMBED_MAPPED flag in _XEMBED_INFO */
    unsigned long xembed_data[2] = {
        0, visible ? X11_XEMBED_MAPPED : 0
    };
    XChangeProperty(tray->x11.dpy, tray->x11.win,
                    tray->x11.xa_xembed_info,
                    tray->x11.xa_xembed_info,
                    32, PropModeReplace,
                    (unsigned char *)xembed_data, 2);
    XFlush(tray->x11.dpy);
    return 0;
}

#endif /* !TRAYCON_NO_X11 */

/* ================================================================== */
/*                                                                      */
/*  PUBLIC API – auto-detection and dispatch                            */
/*                                                                      */
/* ================================================================== */

traycon *traycon_create(const unsigned char *rgba, int width, int height,
                        traycon_click_cb cb, void *userdata)
{
    if (!rgba || width <= 0 || height <= 0)
        return NULL;

    int backend = traycon__preferred_backend;

    /* --- Explicit backend request ---------------------------------- */

#ifndef TRAYCON_NO_SNI
    if (backend == TRAYCON_BACKEND_SNI)
        return sni_try_create(rgba, width, height, cb, userdata);
#endif

#ifndef TRAYCON_NO_X11
    if (backend == TRAYCON_BACKEND_X11)
        return x11_try_create(rgba, width, height, cb, userdata);
#endif

    /* --- Auto-detect ----------------------------------------------- */
    if (backend == TRAYCON_BACKEND_AUTO) {
        traycon *tray = NULL;

#ifndef TRAYCON_NO_SNI
        tray = sni_try_create(rgba, width, height, cb, userdata);
        if (tray) return tray;
#endif

#ifndef TRAYCON_NO_X11
        tray = x11_try_create(rgba, width, height, cb, userdata);
        if (tray) return tray;
#endif

        return NULL;
    }

    /* Unknown backend id */
    return NULL;
}

int traycon_update_icon(traycon *tray, const unsigned char *rgba,
                        int width, int height)
{
    if (!tray || !rgba || width <= 0 || height <= 0)
        return -1;
    return tray->fn_update_icon(tray, rgba, width, height);
}

int traycon_step(traycon *tray)
{
    if (!tray) return -1;
    return tray->fn_step(tray);
}

void traycon_destroy(traycon *tray)
{
    if (!tray) return;
    tray->fn_destroy(tray);
    free(tray);
}

int traycon_set_visible(traycon *tray, int visible)
{
    if (!tray) return -1;
    visible = visible ? 1 : 0;
    if (tray->visible == visible) return 0;
    return tray->fn_set_visible(tray, visible);
}
#endif /* __linux__ */
/* ====== end traycon_linux.c ====== */

/* ====== begin traycon_win32.c ====== */
/*
 * traycon – Windows implementation
 *
 * Uses Shell_NotifyIconW and a message-only window to display a
 * system-tray icon and receive click notifications.
 */
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#define WM_TRAYICON  (WM_APP + 1)
#define TRAY_ICON_ID 1

/* ------------------------------------------------------------------ */
/*  Internal data                                                      */
/* ------------------------------------------------------------------ */

struct traycon {
    HWND             hwnd;
    NOTIFYICONDATAW  nid;
    HICON            hicon;
    traycon_click_cb cb;
    void            *userdata;
    int              visible;   /* non-zero = shown, zero = hidden */
};

static const wchar_t CLASS_NAME[] = L"traycon_wnd";
static int class_registered = 0;

/* ------------------------------------------------------------------ */
/*  Create HICON from RGBA pixel data                                  */
/* ------------------------------------------------------------------ */

static HICON icon_from_rgba(const unsigned char *rgba, int w, int h)
{
    BITMAPV5HEADER bi;
    memset(&bi, 0, sizeof bi);
    bi.bV5Size        = sizeof bi;
    bi.bV5Width       = w;
    bi.bV5Height      = -h;          /* top-down */
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    HDC hdc = GetDC(NULL);
    unsigned char *bits = NULL;
    HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO *)&bi,
                                    DIB_RGB_COLORS, (void **)&bits,
                                    NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!hbmp) return NULL;

    /* RGBA → pre-multiplied BGRA */
    int n = w * h;
    for (int i = 0; i < n; i++) {
        unsigned char r = rgba[i * 4 + 0];
        unsigned char g = rgba[i * 4 + 1];
        unsigned char b = rgba[i * 4 + 2];
        unsigned char a = rgba[i * 4 + 3];
        bits[i * 4 + 0] = (unsigned char)((b * a + 127) / 255);
        bits[i * 4 + 1] = (unsigned char)((g * a + 127) / 255);
        bits[i * 4 + 2] = (unsigned char)((r * a + 127) / 255);
        bits[i * 4 + 3] = a;
    }

    HBITMAP hmask = CreateBitmap(w, h, 1, 1, NULL);
    ICONINFO ii;
    memset(&ii, 0, sizeof ii);
    ii.fIcon    = TRUE;
    ii.hbmMask  = hmask;
    ii.hbmColor = hbmp;

    HICON hicon = CreateIconIndirect(&ii);
    DeleteObject(hbmp);
    DeleteObject(hmask);
    return hicon;
}

/* ------------------------------------------------------------------ */
/*  Window procedure                                                   */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK
wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          (LONG_PTR)cs->lpCreateParams);
        return 0;
    }

    traycon *tray = (traycon *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_TRAYICON && tray) {
        /* lParam = mouse message (uVersion 0) */
        if (lp == WM_LBUTTONUP) {
            if (tray->cb) tray->cb(tray, tray->userdata);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

traycon *traycon_create(const unsigned char *rgba, int width, int height,
                        traycon_click_cb cb, void *userdata)
{
    if (!rgba || width <= 0 || height <= 0)
        return NULL;

    HINSTANCE hinst = GetModuleHandleW(NULL);

    /* Register window class once */
    if (!class_registered) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof wc);
        wc.cbSize        = sizeof wc;
        wc.lpfnWndProc   = wndproc;
        wc.hInstance      = hinst;
        wc.lpszClassName  = CLASS_NAME;
        if (!RegisterClassExW(&wc)) return NULL;
        class_registered = 1;
    }

    traycon *tray = (traycon *)calloc(1, sizeof *tray);
    if (!tray) return NULL;

    tray->cb       = cb;
    tray->userdata = userdata;

    /* Message-only window */
    tray->hwnd = CreateWindowExW(0, CLASS_NAME, L"traycon", 0,
                                  0, 0, 0, 0,
                                  HWND_MESSAGE, NULL, hinst, tray);
    if (!tray->hwnd) { free(tray); return NULL; }

    /* Icon */
    tray->hicon = icon_from_rgba(rgba, width, height);
    if (!tray->hicon) {
        DestroyWindow(tray->hwnd);
        free(tray);
        return NULL;
    }

    /* Shell notification */
    memset(&tray->nid, 0, sizeof tray->nid);
    tray->nid.cbSize           = sizeof tray->nid;
    tray->nid.hWnd             = tray->hwnd;
    tray->nid.uID              = TRAY_ICON_ID;
    tray->nid.uFlags           = NIF_ICON | NIF_MESSAGE;
    tray->nid.uCallbackMessage = WM_TRAYICON;
    tray->nid.hIcon            = tray->hicon;
    if (!Shell_NotifyIconW(NIM_ADD, &tray->nid)) {
        DestroyIcon(tray->hicon);
        DestroyWindow(tray->hwnd);
        free(tray);
        return NULL;
    }
    tray->visible = 1;

    return tray;
}

int traycon_update_icon(traycon *tray, const unsigned char *rgba,
                        int width, int height)
{
    if (!tray || !rgba || width <= 0 || height <= 0)
        return -1;

    HICON newhicon = icon_from_rgba(rgba, width, height);
    if (!newhicon) return -1;

    DestroyIcon(tray->hicon);
    tray->hicon     = newhicon;
    tray->nid.hIcon = newhicon;
    tray->nid.uFlags = NIF_ICON;

    return Shell_NotifyIconW(NIM_MODIFY, &tray->nid) ? 0 : -1;
}

int traycon_step(traycon *tray)
{
    if (!tray) return -1;
    MSG msg;
    while (PeekMessageW(&msg, tray->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

void traycon_destroy(traycon *tray)
{
    if (!tray) return;
    if (tray->visible)
        Shell_NotifyIconW(NIM_DELETE, &tray->nid);
    if (tray->hicon) DestroyIcon(tray->hicon);
    if (tray->hwnd)  DestroyWindow(tray->hwnd);
    free(tray);
}

int traycon_set_visible(traycon *tray, int visible)
{
    if (!tray) return -1;
    visible = visible ? 1 : 0;
    if (tray->visible == visible) return 0;

    if (visible) {
        /* Re-add the icon */
        tray->nid.uFlags = NIF_ICON | NIF_MESSAGE;
        tray->nid.hIcon  = tray->hicon;
        if (!Shell_NotifyIconW(NIM_ADD, &tray->nid)) return -1;
    } else {
        if (!Shell_NotifyIconW(NIM_DELETE, &tray->nid)) return -1;
    }
    tray->visible = visible;
    return 0;
}

void traycon_set_preferred_backend(int backend) { (void)backend; }

#endif /* _WIN32 */
/* ====== end traycon_win32.c ====== */

/* ====== begin traycon_macos.m ====== */
/*
 * traycon – macOS implementation
 *
 * Uses NSStatusBar / NSStatusItem (AppKit) to display a menu-bar icon
 * and receive left-click events.
 *
 * Depends on: -framework Cocoa
 *
 * The translation unit that defines TRAYCON_IMPLEMENTATION must be
 * compiled as Objective-C (i.e. have a .m extension, or be compiled
 * with -x objective-c).
 */
#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal data                                                      */
/* ------------------------------------------------------------------ */

@interface TrayconClickHandler : NSObject
@property (nonatomic, assign) traycon *tray_ptr;
- (void)handleClick:(id)sender;
@end

struct traycon {
    NSStatusItem        *item;     /* retained by NSStatusBar */
    TrayconClickHandler *handler;
    traycon_click_cb     cb;
    void                *userdata;
};

/* ------------------------------------------------------------------ */
/*  Click handler                                                       */
/* ------------------------------------------------------------------ */

@implementation TrayconClickHandler
- (void)handleClick:(id)sender
{
    (void)sender;
    traycon *t = self.tray_ptr;
    if (t && t->cb) t->cb(t, t->userdata);
}
@end

/* ------------------------------------------------------------------ */
/*  RGBA → NSImage conversion                                          */
/* ------------------------------------------------------------------ */

static NSImage *image_from_rgba(const unsigned char *rgba, int w, int h)
{
    NSBitmapImageRep *rep =
        [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:NULL
                          pixelsWide:w
                          pixelsHigh:h
                       bitsPerSample:8
                     samplesPerPixel:4
                            hasAlpha:YES
                            isPlanar:NO
                      colorSpaceName:NSDeviceRGBColorSpace
                         bytesPerRow:w * 4
                        bitsPerPixel:32];
    if (!rep) return nil;

    memcpy([rep bitmapData], rgba, (size_t)w * h * 4);

    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(w, h)];
    [img addRepresentation:rep];
    return img;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

traycon *traycon_create(const unsigned char *rgba, int width, int height,
                        traycon_click_cb cb, void *userdata)
{
    if (!rgba || width <= 0 || height <= 0)
        return NULL;

    /* Ensure an NSApplication instance exists (needed for the status bar). */
    if (!NSApp) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    }

    traycon *tray = (traycon *)calloc(1, sizeof *tray);
    if (!tray) return NULL;

    tray->cb       = cb;
    tray->userdata = userdata;

    NSStatusBar *bar = [NSStatusBar systemStatusBar];
    tray->item = [bar statusItemWithLength:NSSquareStatusItemLength];
    if (!tray->item) { free(tray); return NULL; }

    NSImage *img = image_from_rgba(rgba, width, height);
    if (!img) {
        [bar removeStatusItem:tray->item];
        free(tray);
        return NULL;
    }
    [img setTemplate:NO];
    CGFloat thickness = [bar thickness];
    [img setSize:NSMakeSize(thickness, thickness)];
    tray->item.button.image = img;

    tray->handler             = [[TrayconClickHandler alloc] init];
    tray->handler.tray_ptr    = tray;
    tray->item.button.target  = tray->handler;
    tray->item.button.action  = @selector(handleClick:);
    [tray->item.button sendActionOn:NSEventMaskLeftMouseUp];

    return tray;
}

int traycon_update_icon(traycon *tray, const unsigned char *rgba,
                        int width, int height)
{
    if (!tray || !rgba || width <= 0 || height <= 0)
        return -1;

    NSImage *img = image_from_rgba(rgba, width, height);
    if (!img) return -1;

    [img setTemplate:NO];
    CGFloat thickness = [[NSStatusBar systemStatusBar] thickness];
    [img setSize:NSMakeSize(thickness, thickness)];
    tray->item.button.image = img;
    return 0;
}

int traycon_step(traycon *tray)
{
    if (!tray) return -1;

    /* Drain pending AppKit events without blocking. */
    @autoreleasepool {
        NSEvent *ev;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:nil
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES]) != nil) {
            [NSApp sendEvent:ev];
        }
    }
    return 0;
}

void traycon_destroy(traycon *tray)
{
    if (!tray) return;
    if (tray->item)
        [[NSStatusBar systemStatusBar] removeStatusItem:tray->item];
    tray->item    = nil;
    tray->handler = nil;
    free(tray);
}

int traycon_set_visible(traycon *tray, int visible)
{
    if (!tray || !tray->item) return -1;
    tray->item.visible = visible ? YES : NO;
    return 0;
}

void traycon_set_preferred_backend(int backend) { (void)backend; }

#endif /* __APPLE__ */
/* ====== end traycon_macos.m ====== */

#endif /* TRAYCON_IMPLEMENTATION_GUARD */
#endif /* TRAYCON_IMPLEMENTATION */

#endif /* TRAYCON_H */
