//
// "$Id: sql.h 1362 2018-06-30 13:48:10 $"
//
// sql.h -- encapsulation of sqlite3 routins for sqlTable
//
// Copyright 2017-2018 by Yongchao Fan.
//
// This library is free software distributed under GUN GPL 3.0,
// see the license at:
//
//     https://github.com/zoudaokou/flTable/blob/master/LICENSE
//
// Please report all bugs and problems on the following page:
//
//     https://github.com/zoudaokou/flTable/issues/new
//
#include <sqlite3.h>
typedef int (*sqlite3_callback)(
   void *,    /* Data provided in the 4th argument of sqlite3_exec() */
   int,       /* The number of columns in row */
   char **,   /* An array of strings representing fields in the row */
   char **    /* An array of strings representing column names */
);
typedef  void(*hook_callback)(
    void *, // Data provided in the 3rd argument of sqlite3_update_hook
    int,    //SQLITE_INSERT, SQLITE_UPDATE or SQLITE_DELETE
    char const *,   //database name
    char const *,   //table name
    sqlite3_int64   //rowid
);
int sql_open(const char *fn);
int sql_save(const char *fn);
int sql_close();
int sql_exec( const char *sql, sqlite3_callback sql_cb, void *data );
void * sql_hook(hook_callback hook_cb, void *data);
int sql_queue(const char *fmt, ...);
int sql_commit();
int sql_row(char *sql);
int sql_table(const char *sql, char **preply);
const char *sql_errmsg();