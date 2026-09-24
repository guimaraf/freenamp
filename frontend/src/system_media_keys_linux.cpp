#include "system_media_keys.hpp"

#ifndef _WIN32
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

// Definições de protocolo D-Bus necessárias para MPRIS v2
#define DBUS_BUS_SESSION 0
#define DBUS_NAME_FLAG_REPLACE_EXISTING 0x2
#define DBUS_NAME_FLAG_DO_NOT_QUEUE 0x4

#define DBUS_MESSAGE_TYPE_METHOD_CALL 1
#define DBUS_MESSAGE_TYPE_METHOD_RETURN 2
#define DBUS_MESSAGE_TYPE_ERROR 3
#define DBUS_MESSAGE_TYPE_SIGNAL 4

#define DBUS_TYPE_INVALID 0
#define DBUS_TYPE_BOOLEAN 'b'
#define DBUS_TYPE_INT64 'x'
#define DBUS_TYPE_STRING 's'
#define DBUS_TYPE_OBJECT_PATH 'o'
#define DBUS_TYPE_ARRAY 'a'
#define DBUS_TYPE_VARIANT 'v'
#define DBUS_TYPE_DICT_ENTRY 'e'

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef uint32_t dbus_bool_t;
typedef uint32_t dbus_uint32_t;
typedef int64_t dbus_int64_t;

struct DBusError {
    const char *name;
    const char *message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void *padding1;
};

struct DBusMessageIter {
    void *dummy1;
    void *dummy2;
    dbus_uint32_t dummy3;
    int dummy4;
    int dummy5;
    int dummy6;
    int dummy7;
    int dummy8;
    int dummy9;
    int dummy10;
    int dummy11;
    int pad1;
    void *pad2;
    void *pad3;
};

namespace freenamp::frontend {

namespace {

// Tabela de ponteiros de funções para carregamento dinâmico via dlopen
struct DBusApi {
    void* handle = nullptr;

    void (*error_init)(DBusError *error) = nullptr;
    void (*error_free)(DBusError *error) = nullptr;
    dbus_bool_t (*error_is_set)(const DBusError *error) = nullptr;

    DBusConnection* (*bus_get)(int bus_type, DBusError *error) = nullptr;
    int (*bus_request_name)(DBusConnection *connection, const char *name, unsigned int flags, DBusError *error) = nullptr;
    void (*connection_flush)(DBusConnection *connection) = nullptr;
    dbus_bool_t (*connection_read_write)(DBusConnection *connection, int timeout_milliseconds) = nullptr;
    DBusMessage* (*connection_pop_message)(DBusConnection *connection) = nullptr;
    dbus_bool_t (*connection_send)(DBusConnection *connection, DBusMessage *message, dbus_uint32_t *client_serial) = nullptr;
    void (*connection_unref)(DBusConnection *connection) = nullptr;

    int (*message_get_type)(DBusMessage *message) = nullptr;
    const char* (*message_get_path)(DBusMessage *message) = nullptr;
    const char* (*message_get_interface)(DBusMessage *message) = nullptr;
    const char* (*message_get_member)(DBusMessage *message) = nullptr;
    DBusMessage* (*message_new_method_return)(DBusMessage *method_call) = nullptr;
    DBusMessage* (*message_new_signal)(const char *path, const char *iface, const char *name) = nullptr;
    DBusMessage* (*message_new_error)(DBusMessage *reply_to, const char *error_name, const char *error_message) = nullptr;
    void (*message_unref)(DBusMessage *message) = nullptr;

    void (*message_iter_init_append)(DBusMessage *message, DBusMessageIter *iter) = nullptr;
    dbus_bool_t (*message_iter_init)(DBusMessage *message, DBusMessageIter *iter) = nullptr;
    int (*message_iter_get_arg_type)(DBusMessageIter *iter) = nullptr;
    void (*message_iter_get_basic)(DBusMessageIter *iter, void *value) = nullptr;
    dbus_bool_t (*message_iter_next)(DBusMessageIter *iter) = nullptr;
    dbus_bool_t (*message_iter_append_basic)(DBusMessageIter *iter, int type, const void *value) = nullptr;
    dbus_bool_t (*message_iter_open_container)(DBusMessageIter *iter, int type, const char *contained_signature, DBusMessageIter *sub) = nullptr;
    dbus_bool_t (*message_iter_close_container)(DBusMessageIter *iter, DBusMessageIter *sub) = nullptr;

    bool load() {
        handle = dlopen("libdbus-1.so.3", RTLD_LAZY);
        if (!handle) {
            handle = dlopen("libdbus-1.so", RTLD_LAZY);
        }
        if (!handle) {
            return false;
        }

        #define LOAD_SYM(name) \
            name = reinterpret_cast<decltype(name)>(dlsym(handle, "dbus_" #name)); \
            if (!name) return false;

        LOAD_SYM(error_init);
        LOAD_SYM(error_free);
        LOAD_SYM(error_is_set);
        LOAD_SYM(bus_get);
        LOAD_SYM(bus_request_name);
        LOAD_SYM(connection_flush);
        LOAD_SYM(connection_read_write);
        LOAD_SYM(connection_pop_message);
        LOAD_SYM(connection_send);
        LOAD_SYM(connection_unref);
        LOAD_SYM(message_get_type);
        LOAD_SYM(message_get_path);
        LOAD_SYM(message_get_interface);
        LOAD_SYM(message_get_member);
        LOAD_SYM(message_new_method_return);
        LOAD_SYM(message_new_signal);
        LOAD_SYM(message_new_error);
        LOAD_SYM(message_unref);
        LOAD_SYM(message_iter_init_append);
        LOAD_SYM(message_iter_init);
        LOAD_SYM(message_iter_get_arg_type);
        LOAD_SYM(message_iter_get_basic);
        LOAD_SYM(message_iter_next);
        LOAD_SYM(message_iter_append_basic);
        LOAD_SYM(message_iter_open_container);
        LOAD_SYM(message_iter_close_container);
        #undef LOAD_SYM

        return true;
    }

    void unload() {
        if (handle) {
            dlclose(handle);
            handle = nullptr;
        }
    }
};

const char* MPRIS_INTROSPECTION_XML =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
    "    <method name=\"Get\">\n"
    "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetAll\">\n"
    "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"props\" type=\"a{sv}\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <signal name=\"PropertiesChanged\">\n"
    "      <arg name=\"interface_name\" type=\"s\"/>\n"
    "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
    "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name=\"org.mpris.MediaPlayer2\">\n"
    "    <method name=\"Raise\"/>\n"
    "    <method name=\"Quit\"/>\n"
    "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
    "  </interface>\n"
    "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
    "    <method name=\"Next\"/>\n"
    "    <method name=\"Previous\"/>\n"
    "    <method name=\"Pause\"/>\n"
    "    <method name=\"PlayPause\"/>\n"
    "    <method name=\"Stop\"/>\n"
    "    <method name=\"Play\"/>\n"
    "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
    "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
    "  </interface>\n"
    "</node>\n";

} // namespace

class SystemMediaKeysLinux final : public ISystemMediaKeys {
public:
    SystemMediaKeysLinux() = default;
    ~SystemMediaKeysLinux() override {
        shutdown();
    }

    bool init(SDL_Window* window, backend::CoreController& core) override {
        m_window = window;
        m_core = &core;

        if (!m_dbus.load()) {
            std::cout << "[SystemMediaKeysLinux] libdbus-1 nao disponivel. Fallback limpo ativado.\n";
            return false;
        }

        DBusError err;
        m_dbus.error_init(&err);

        m_conn = m_dbus.bus_get(DBUS_BUS_SESSION, &err);
        if (m_dbus.error_is_set(&err) || !m_conn) {
            std::cout << "[SystemMediaKeysLinux] Falha ao conectar ao D-Bus de sessao: " 
                      << (err.message ? err.message : "Desconhecido") << "\n";
            m_dbus.error_free(&err);
            return false;
        }

        int req_res = m_dbus.bus_request_name(
            m_conn,
            "org.mpris.MediaPlayer2.freenamp",
            DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE,
            &err
        );

        if (m_dbus.error_is_set(&err)) {
            std::cout << "[SystemMediaKeysLinux] Falha ao registrar nome MPRIS no D-Bus: " 
                      << (err.message ? err.message : "") << "\n";
            m_dbus.error_free(&err);
            return false;
        }

        if (req_res != 1 && req_res != 4) { // PRIMARY_OWNER or ALREADY_OWNER
            std::cout << "[SystemMediaKeysLinux] Nome org.mpris.MediaPlayer2.freenamp ja ocupado.\n";
        } else {
            std::cout << "[SystemMediaKeysLinux] Servico MPRIS v2 registrado com sucesso via D-Bus!\n";
        }

        m_initialized = true;
        return true;
    }

    void update() override {
        if (!m_initialized || !m_conn) return;

        // Leitura e despacho nao-bloqueante de mensagens pendentes do D-Bus
        m_dbus.connection_read_write(m_conn, 0);

        while (DBusMessage* msg = m_dbus.connection_pop_message(m_conn)) {
            handle_message(msg);
            m_dbus.message_unref(msg);
        }
    }

    void update_metadata(const std::string& title,
                         const std::string& artist,
                         int duration_sec,
                         backend::PlaybackState state) override {
        m_title = title.empty() ? "Freenamp" : title;
        m_artist = artist;
        m_duration_sec = duration_sec;
        m_state = state;

        if (m_initialized && m_conn) {
            emit_properties_changed();
        }
    }

    void shutdown() override {
        if (m_initialized) {
            if (m_conn) {
                m_dbus.connection_unref(m_conn);
                m_conn = nullptr;
            }
            m_dbus.unload();
            m_initialized = false;
            m_core = nullptr;
            m_window = nullptr;
        }
    }

private:
    SDL_Window* m_window = nullptr;
    backend::CoreController* m_core = nullptr;
    DBusApi m_dbus;
    DBusConnection* m_conn = nullptr;
    bool m_initialized = false;

    std::string m_title = "Freenamp";
    std::string m_artist = "";
    int m_duration_sec = 0;
    backend::PlaybackState m_state = backend::PlaybackState::Stopped;

    std::string get_playback_status_str() const {
        switch (m_state) {
            case backend::PlaybackState::Playing:
                return "Playing";
            case backend::PlaybackState::Paused:
                return "Paused";
            default:
                return "Stopped";
        }
    }

    void append_dict_entry_string(DBusMessageIter* dict, const char* key, const char* value) {
        DBusMessageIter entry;
        m_dbus.message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        m_dbus.message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

        DBusMessageIter var;
        m_dbus.message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &var);
        m_dbus.message_iter_append_basic(&var, DBUS_TYPE_STRING, &value);
        m_dbus.message_iter_close_container(&entry, &var);

        m_dbus.message_iter_close_container(dict, &entry);
    }

    void append_dict_entry_bool(DBusMessageIter* dict, const char* key, dbus_bool_t value) {
        DBusMessageIter entry;
        m_dbus.message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        m_dbus.message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

        DBusMessageIter var;
        m_dbus.message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
        m_dbus.message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &value);
        m_dbus.message_iter_close_container(&entry, &var);

        m_dbus.message_iter_close_container(dict, &entry);
    }

    void append_dict_entry_metadata(DBusMessageIter* dict) {
        DBusMessageIter entry;
        m_dbus.message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        const char* key = "Metadata";
        m_dbus.message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

        DBusMessageIter var;
        m_dbus.message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a{sv}", &var);

        DBusMessageIter meta_arr;
        m_dbus.message_iter_open_container(&var, DBUS_TYPE_ARRAY, "{sv}", &meta_arr);

        // mpris:trackid (object path)
        {
            DBusMessageIter me;
            m_dbus.message_iter_open_container(&meta_arr, DBUS_TYPE_DICT_ENTRY, nullptr, &me);
            const char* tk = "mpris:trackid";
            const char* tv = "/org/mpris/MediaPlayer2/CurrentTrack";
            m_dbus.message_iter_append_basic(&me, DBUS_TYPE_STRING, &tk);
            DBusMessageIter mv;
            m_dbus.message_iter_open_container(&me, DBUS_TYPE_VARIANT, "o", &mv);
            m_dbus.message_iter_append_basic(&mv, DBUS_TYPE_OBJECT_PATH, &tv);
            m_dbus.message_iter_close_container(&me, &mv);
            m_dbus.message_iter_close_container(&meta_arr, &me);
        }

        // xesam:title (string)
        append_dict_entry_string(&meta_arr, "xesam:title", m_title.c_str());

        // xesam:artist (array of strings)
        {
            DBusMessageIter me;
            m_dbus.message_iter_open_container(&meta_arr, DBUS_TYPE_DICT_ENTRY, nullptr, &me);
            const char* ak = "xesam:artist";
            m_dbus.message_iter_append_basic(&me, DBUS_TYPE_STRING, &ak);
            DBusMessageIter mv;
            m_dbus.message_iter_open_container(&me, DBUS_TYPE_VARIANT, "as", &mv);
            DBusMessageIter arr_str;
            m_dbus.message_iter_open_container(&mv, DBUS_TYPE_ARRAY, "s", &arr_str);
            if (!m_artist.empty()) {
                const char* art_val = m_artist.c_str();
                m_dbus.message_iter_append_basic(&arr_str, DBUS_TYPE_STRING, &art_val);
            }
            m_dbus.message_iter_close_container(&mv, &arr_str);
            m_dbus.message_iter_close_container(&me, &mv);
            m_dbus.message_iter_close_container(&meta_arr, &me);
        }

        // mpris:length (int64 microseconds)
        {
            DBusMessageIter me;
            m_dbus.message_iter_open_container(&meta_arr, DBUS_TYPE_DICT_ENTRY, nullptr, &me);
            const char* lk = "mpris:length";
            dbus_int64_t len_us = static_cast<dbus_int64_t>(m_duration_sec) * 1000000LL;
            m_dbus.message_iter_append_basic(&me, DBUS_TYPE_STRING, &lk);
            DBusMessageIter mv;
            m_dbus.message_iter_open_container(&me, DBUS_TYPE_VARIANT, "x", &mv);
            m_dbus.message_iter_append_basic(&mv, DBUS_TYPE_INT64, &len_us);
            m_dbus.message_iter_close_container(&me, &mv);
            m_dbus.message_iter_close_container(&meta_arr, &me);
        }

        m_dbus.message_iter_close_container(&var, &meta_arr);
        m_dbus.message_iter_close_container(&entry, &var);
        m_dbus.message_iter_close_container(dict, &entry);
    }

    void emit_properties_changed() {
        DBusMessage* sig = m_dbus.message_new_signal(
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged"
        );
        if (!sig) return;

        DBusMessageIter iter;
        m_dbus.message_iter_init_append(sig, &iter);

        const char* iface = "org.mpris.MediaPlayer2.Player";
        m_dbus.message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);

        DBusMessageIter changed_dict;
        m_dbus.message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &changed_dict);
        append_dict_entry_string(&changed_dict, "PlaybackStatus", get_playback_status_str().c_str());
        append_dict_entry_metadata(&changed_dict);
        m_dbus.message_iter_close_container(&iter, &changed_dict);

        DBusMessageIter empty_arr;
        m_dbus.message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &empty_arr);
        m_dbus.message_iter_close_container(&iter, &empty_arr);

        m_dbus.connection_send(m_conn, sig, nullptr);
        m_dbus.connection_flush(m_conn);
        m_dbus.message_unref(sig);
    }

    void handle_message(DBusMessage* msg) {
        if (m_dbus.message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return;
        }

        const char* path = m_dbus.message_get_path(msg);
        const char* iface = m_dbus.message_get_interface(msg);
        const char* member = m_dbus.message_get_member(msg);

        if (!path || !member) return;

        std::string s_path = path;
        std::string s_iface = iface ? iface : "";
        std::string s_member = member;

        if (s_path != "/org/mpris/MediaPlayer2") {
            return;
        }

        // 1. Introspection
        if (s_iface == "org.freedesktop.DBus.Introspectable" && s_member == "Introspect") {
            DBusMessage* reply = m_dbus.message_new_method_return(msg);
            if (reply) {
                DBusMessageIter args;
                m_dbus.message_iter_init_append(reply, &args);
                const char* xml = MPRIS_INTROSPECTION_XML;
                m_dbus.message_iter_append_basic(&args, DBUS_TYPE_STRING, &xml);
                m_dbus.connection_send(m_conn, reply, nullptr);
                m_dbus.connection_flush(m_conn);
                m_dbus.message_unref(reply);
            }
            return;
        }

        // 2. Properties (Get & GetAll)
        if (s_iface == "org.freedesktop.DBus.Properties") {
            if (s_member == "Get") {
                DBusMessageIter in_iter;
                const char* target_iface = nullptr;
                const char* target_prop = nullptr;
                if (m_dbus.message_iter_init(msg, &in_iter)) {
                    m_dbus.message_iter_get_basic(&in_iter, &target_iface);
                    if (m_dbus.message_iter_next(&in_iter)) {
                        m_dbus.message_iter_get_basic(&in_iter, &target_prop);
                    }
                }

                DBusMessage* reply = m_dbus.message_new_method_return(msg);
                if (reply) {
                    DBusMessageIter out_iter;
                    m_dbus.message_iter_init_append(reply, &out_iter);

                    std::string p = target_prop ? target_prop : "";
                    if (p == "PlaybackStatus") {
                        DBusMessageIter var;
                        m_dbus.message_iter_open_container(&out_iter, DBUS_TYPE_VARIANT, "s", &var);
                        std::string st = get_playback_status_str();
                        const char* st_c = st.c_str();
                        m_dbus.message_iter_append_basic(&var, DBUS_TYPE_STRING, &st_c);
                        m_dbus.message_iter_close_container(&out_iter, &var);
                    } else if (p == "Identity") {
                        DBusMessageIter var;
                        m_dbus.message_iter_open_container(&out_iter, DBUS_TYPE_VARIANT, "s", &var);
                        const char* id_c = "Freenamp";
                        m_dbus.message_iter_append_basic(&var, DBUS_TYPE_STRING, &id_c);
                        m_dbus.message_iter_close_container(&out_iter, &var);
                    } else if (p == "CanControl" || p == "CanPlay" || p == "CanPause" || p == "CanGoNext" || p == "CanGoPrevious" || p == "CanQuit" || p == "CanRaise") {
                        DBusMessageIter var;
                        m_dbus.message_iter_open_container(&out_iter, DBUS_TYPE_VARIANT, "b", &var);
                        dbus_bool_t b_true = 1;
                        m_dbus.message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b_true);
                        m_dbus.message_iter_close_container(&out_iter, &var);
                    }

                    m_dbus.connection_send(m_conn, reply, nullptr);
                    m_dbus.connection_flush(m_conn);
                    m_dbus.message_unref(reply);
                }
                return;
            }

            if (s_member == "GetAll") {
                DBusMessage* reply = m_dbus.message_new_method_return(msg);
                if (reply) {
                    DBusMessageIter out_iter;
                    m_dbus.message_iter_init_append(reply, &out_iter);

                    DBusMessageIter dict;
                    m_dbus.message_iter_open_container(&out_iter, DBUS_TYPE_ARRAY, "{sv}", &dict);

                    append_dict_entry_string(&dict, "PlaybackStatus", get_playback_status_str().c_str());
                    append_dict_entry_string(&dict, "Identity", "Freenamp");
                    append_dict_entry_bool(&dict, "CanControl", 1);
                    append_dict_entry_bool(&dict, "CanPlay", 1);
                    append_dict_entry_bool(&dict, "CanPause", 1);
                    append_dict_entry_bool(&dict, "CanGoNext", 1);
                    append_dict_entry_bool(&dict, "CanGoPrevious", 1);
                    append_dict_entry_bool(&dict, "CanQuit", 1);
                    append_dict_entry_bool(&dict, "CanRaise", 1);
                    append_dict_entry_metadata(&dict);

                    m_dbus.message_iter_close_container(&out_iter, &dict);

                    m_dbus.connection_send(m_conn, reply, nullptr);
                    m_dbus.connection_flush(m_conn);
                    m_dbus.message_unref(reply);
                }
                return;
            }
        }

        // 3. Player Actions
        if (s_iface == "org.mpris.MediaPlayer2.Player") {
            if (s_member == "PlayPause") {
                if (m_core) m_core->toggle_pause();
            } else if (s_member == "Play") {
                if (m_core) m_core->play();
            } else if (s_member == "Pause") {
                if (m_core) m_core->pause();
            } else if (s_member == "Stop") {
                if (m_core) m_core->stop();
            } else if (s_member == "Next") {
                if (m_core) m_core->next();
            } else if (s_member == "Previous") {
                if (m_core) m_core->previous();
            }

            DBusMessage* reply = m_dbus.message_new_method_return(msg);
            if (reply) {
                m_dbus.connection_send(m_conn, reply, nullptr);
                m_dbus.connection_flush(m_conn);
                m_dbus.message_unref(reply);
            }
            return;
        }

        // 4. Root Org.mpris.MediaPlayer2 Actions
        if (s_iface == "org.mpris.MediaPlayer2") {
            if (s_member == "Raise") {
                if (m_window) {
                    SDL_RaiseWindow(m_window);
                }
            } else if (s_member == "Quit") {
                SDL_Event quit_ev;
                quit_ev.type = SDL_QUIT;
                SDL_PushEvent(&quit_ev);
            }

            DBusMessage* reply = m_dbus.message_new_method_return(msg);
            if (reply) {
                m_dbus.connection_send(m_conn, reply, nullptr);
                m_dbus.connection_flush(m_conn);
                m_dbus.message_unref(reply);
            }
            return;
        }
    }
};

std::unique_ptr<ISystemMediaKeys> create_system_media_keys() {
    return std::make_unique<SystemMediaKeysLinux>();
}

} // namespace freenamp::frontend
#else
// Stub para compiladores em ambiente Windows caso o arquivo seja indevidamente referenciado
namespace freenamp::frontend {
std::unique_ptr<ISystemMediaKeys> create_system_media_keys();
}
#endif
