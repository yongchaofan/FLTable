//
// "$Id: sql.cxx 4087 2018-06-30 13:48:10 $"
//
// sql.cxx -- encapsulation of sqlite3 routins for sqlTable
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
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "sql.h"
void log_print(const char *name, const char *msg, int len);

#include <mutex>
#include <queue>
#include <string>

std::mutex db_mutex;
std::queue<std::string> db_queue;
sqlite3 *db_read=NULL, *db_write=NULL;

int sql_open(const char *fn)
{
	if ( db_read!=NULL ) sqlite3_close(db_read);
	if ( db_write!=NULL ) sqlite3_close(db_write);
	char uri[4096];
	sprintf(uri, "file:%s?cache=shared", fn);
 	return (sqlite3_open(uri, &db_read )==SQLITE_OK &&
			sqlite3_open(uri, &db_write)==SQLITE_OK );
}
int sql_close()
{
	sql_commit();
	sqlite3_close(db_read);
	sqlite3_close(db_write);
	return 0;
}
int sql_save(const char *fn)
{
	int rc = 0;
	sqlite3 *bdb;
	sqlite3_backup *pBackup;
	if( sqlite3_open(fn, &bdb)==SQLITE_OK ) {
		pBackup = sqlite3_backup_init(bdb, "main", db_read, "main");
		if( pBackup ) {
			sqlite3_backup_step(pBackup, -1);
			rc = sqlite3_backup_pagecount(pBackup);
			sqlite3_backup_finish(pBackup);
		}
		sqlite3_close(bdb);
	}
	return rc;
}
int sql_exec(const char *sql, sqlite3_callback sql_cb, void *data)
{
	return sqlite3_exec(db_write, sql, sql_cb, data, NULL)==SQLITE_OK;
}
void *sql_hook(hook_callback hook_cb, void *data)
{
	return sqlite3_update_hook(db_write, hook_cb, data);
}
int sql_queue(const char *fmt, ...)
{
	char buf[8192];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, 8192, (char *)fmt, args);
	va_end(args);
	if ( *buf=='C' ) return sql_commit();

	db_mutex.lock();
	db_queue.push(std::string(buf));
	db_mutex.unlock();
	return true;
}
int sql_commit( )
{
	int rc = false;
	db_mutex.lock();
	if ( !db_queue.empty() ) {;
		sqlite3_exec(db_write, "BEGIN TRANSACTION", NULL, NULL, NULL);
		while ( !db_queue.empty() ) {
			std::string sql = db_queue.front();
			rc = (sqlite3_exec(db_write,sql.c_str(),NULL,NULL,NULL)==SQLITE_OK);
			log_print(rc?"+++":"---", sql.c_str(), sql.length());
			db_queue.pop();
		}
		sqlite3_exec(db_write, "END TRANSACTION", NULL, NULL, NULL);
	}
	db_mutex.unlock();
	return rc;
}
int sql_row(char *sql)
{
	int len = 0;
	sqlite3_stmt *res;
	if( sqlite3_prepare_v2(db_read, sql, 1024, &res, NULL)==SQLITE_OK ) {
		int c = sqlite3_column_count(res);
		if ( sqlite3_step(res)==SQLITE_ROW ) {
			for ( int i=0; i<c; i++ )
				len += sprintf(sql+len, "%s ", sqlite3_column_text(res, i));
			sql[--len] = 0;
		}
	}
	sqlite3_finalize(res);
	return len;
}
int sql_table(const char *sql, char **preply)
{
	*preply = NULL;
	if ( strncmp(sql, "select ", 7)!=0 ) {
		if (sqlite3_exec(db_write, sql, NULL, NULL, NULL)!=SQLITE_OK) {
			*preply = strdup(sqlite3_errmsg(db_write));
			if ( *preply!=NULL )
				return strlen(*preply);
		}
		return 0;
	}

	sqlite3_stmt *res;
	if ( sqlite3_prepare_v2(db_read, sql, 1024, &res, NULL)!=SQLITE_OK ) {
		*preply = strdup(sqlite3_errmsg(db_read));
		if ( *preply!=NULL )
			return strlen(*preply);
		else
			return 0;
	}

	size_t size = 8192, len = 0;
	char *buf = (char *)malloc(size);
	int c = sqlite3_column_count(res);
	for ( int i=0; i<c; i++ )
		len += sprintf(buf+len, "%s\t", sqlite3_column_name(res, i));

	while ( sqlite3_step(res)==SQLITE_ROW ) {
		len += sprintf(buf+len-1, "\n")-1;
		for ( int i=0; i<c; i++ ) {
			char *p = (char *)sqlite3_column_text(res, i);
			if ( p==NULL ) p=(char *)"";
			if ( len+strlen(p)+2>size ) {
				size*=2;
				char *buf2 = (char *)realloc(buf, size);
				if ( buf2==NULL ) goto done_select;
				buf = buf2;
			}
			len+=sprintf(buf+len, "%s\t", p);
		}
	}
done_select:
	sqlite3_finalize(res);
	if ( len>0 ) {
		buf[--len]=0;
		*preply = buf;
	}
	return len;
}
const char *sql_errmsg()
{
	return sqlite3_errmsg(db_write);
}