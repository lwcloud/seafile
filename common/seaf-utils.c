#include "common.h"

#include "log.h"

#include "seafile-session.h"
#include "seaf-utils.h"
#include "seaf-db.h"

#include <stdlib.h>
#include <string.h>

char *
seafile_session_get_tmp_file_path (SeafileSession *session,
                                   const char *basename,
                                   char path[])
{
    int path_len;

    path_len = strlen (session->tmp_file_dir);
    memcpy (path, session->tmp_file_dir, path_len + 1);
    path[path_len] = '/';
    strcpy (path + path_len + 1, basename);

    return path;
}

#ifdef SEAFILE_SERVER

#define SQLITE_DB_NAME "seafile.db"

#define DEFAULT_MAX_CONNECTIONS 100

static int
sqlite_db_start (SeafileSession *session)
{
    char *db_path;
    int max_connections = 0;

    max_connections = g_key_file_get_integer (session->config,
                                              "database", "max_connections",
                                              NULL);
    if (max_connections <= 0)
        max_connections = DEFAULT_MAX_CONNECTIONS;

    db_path = g_build_filename (session->seaf_dir, SQLITE_DB_NAME, NULL);
    session->db = seaf_db_new_sqlite (db_path, max_connections);
    if (!session->db) {
        seaf_warning ("Failed to start sqlite db.\n");
        return -1;
    }

    return 0;
}

#define MYSQL_DEFAULT_PORT "3306"

static int
mysql_db_start (SeafileSession *session)
{
    char *host, *port, *user, *passwd, *db, *unix_socket, *charset;
    gboolean use_ssl = FALSE;
    int max_connections = 0;
    GError *error = NULL;

    host = g_key_file_get_string (session->config, "database", "host", &error);
    if (!host) {
        seaf_warning ("DB host not set in config.\n");
        return -1;
    }

    port = g_key_file_get_string (session->config, "database", "port", &error);
    if (!port) {
        port = g_strdup(MYSQL_DEFAULT_PORT);
    }

    user = g_key_file_get_string (session->config, "database", "user", &error);
    if (!user) {
        seaf_warning ("DB user not set in config.\n");
        return -1;
    }

    passwd = g_key_file_get_string (session->config, "database", "password", &error);
    if (!passwd) {
        seaf_warning ("DB passwd not set in config.\n");
        return -1;
    }

    db = g_key_file_get_string (session->config, "database", "db_name", &error);
    if (!db) {
        seaf_warning ("DB name not set in config.\n");
        return -1;
    }

    unix_socket = g_key_file_get_string (session->config, 
                                         "database", "unix_socket", NULL);

    use_ssl = g_key_file_get_boolean (session->config,
                                      "database", "use_ssl", NULL);

    charset = g_key_file_get_string (session->config,
                                     "database", "connection_charset", NULL);

    max_connections = g_key_file_get_integer (session->config,
                                              "database", "max_connections",
                                              NULL);
    if (max_connections <= 0)
        max_connections = DEFAULT_MAX_CONNECTIONS;

    session->db = seaf_db_new_mysql (host, port, user, passwd, db, unix_socket, use_ssl, charset, max_connections);
    if (!session->db) {
        seaf_warning ("Failed to start mysql db.\n");
        return -1;
    }

    g_free (host);
    g_free (port);
    g_free (user);
    g_free (passwd);
    g_free (db);
    g_free (unix_socket);
    g_free (charset);
    if (error)
        g_clear_error (&error);

    return 0;
}

static int
pgsql_db_start (SeafileSession *session)
{
    char *host, *user, *passwd, *db, *unix_socket;
    GError *error = NULL;

    host = g_key_file_get_string (session->config, "database", "host", &error);
    if (!host) {
        seaf_warning ("DB host not set in config.\n");
        return -1;
    }

    user = g_key_file_get_string (session->config, "database", "user", &error);
    if (!user) {
        seaf_warning ("DB user not set in config.\n");
        return -1;
    }

    passwd = g_key_file_get_string (session->config, "database", "password", &error);
    if (!passwd) {
        seaf_warning ("DB passwd not set in config.\n");
        return -1;
    }

    db = g_key_file_get_string (session->config, "database", "db_name", &error);
    if (!db) {
        seaf_warning ("DB name not set in config.\n");
        return -1;
    }

    unix_socket = g_key_file_get_string (session->config,
                                         "database", "unix_socket", &error);

    session->db = seaf_db_new_pgsql (host, user, passwd, db, unix_socket);
    if (!session->db) {
        seaf_warning ("Failed to start pgsql db.\n");
        return -1;
    }

    g_free (host);
    g_free (user);
    g_free (passwd);
    g_free (db);
    g_free (unix_socket);

    return 0;
}

int
load_database_config (SeafileSession *session)
{
    char *type;
    GError *error = NULL;
    int ret = 0;

    type = g_key_file_get_string (session->config, "database", "type", &error);
    /* Default to use sqlite if not set. */
    if (!type || strcasecmp (type, "sqlite") == 0) {
        ret = sqlite_db_start (session);
    } else if (strcasecmp (type, "mysql") == 0) {
        ret = mysql_db_start (session);
    } else if (strcasecmp (type, "pgsql") == 0) {
        ret = pgsql_db_start (session);
    } else {
        seaf_warning ("Unsupported db type %s.\n", type);
        ret = -1;
    }

    g_free (type);

    return ret;
}

#endif
