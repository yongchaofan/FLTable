//
// "$Id: flTable 8998 2019-04-11 13:48:10 $"
//
// flTable -- A sqlite3 frontend based on FLTK
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
const char *ABOUT_TABLE="\n\n\n\
flTable is a SQLite3 frontend based on FLTK 1.4\n\n\
  * Fl_Table based user interface\n\n\
  * Double click on data cell to filter\n\n\
  * Double click on column header to sort\n\n\
  * Right click to insert, update or delete\n\n\
  * Copy to and paste from text or spreadsheet\n\n\
  * Scripting interface xmlhttp://127.0.0.1:%d\n\n\n\
by yongchaofan@@gmail.com	08-18-2018\n\n\
https://github.com/zoudaokou/flTable\n";
#ifdef WIN32
	#include <direct.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#define socklen_t int
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#define closesocket close
#endif
#include <stdio.h>
#include "sql.h"
#include "sqlTable.h"

#include <thread>
int httpd_init();
void httpd_exit();

#ifdef __APPLE__
	#define MENUHEIGHT 0
#else
	#define MENUHEIGHT 32
#endif
#define CMDHEIGHT 24
#include <FL/x.H>               // needed for fl_display
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "Fl_Browser_Input.h"
Fl_Sys_Menu_Bar *pMenu;
Fl_Browser_Input *pCmd;
Fl_Window *pTableWin;
sqlTable *pTable;

static Fl_Native_File_Chooser fnfc;
const char *file_chooser(const char *title, const char *filter,
					int type=Fl_Native_File_Chooser::BROWSE_SAVE_FILE)
{
	fnfc.title(title);
	fnfc.filter(filter);
	fnfc.directory(".");           		// default directory to use
	fnfc.type(type);
	switch ( fnfc.show() ) {			// Show native chooser
		case -1:  			 			// ERROR
		case  1: return NULL;  			// CANCEL
		default: return fnfc.filename();// FILE CHOSEN
	}
}

void cmd_callback(Fl_Widget *w) {
	char buf[1024];
	strncpy(buf, pCmd->value(), 1023);
	buf[1023] = 0;
	pCmd->add( buf );
	pCmd->position(strlen(buf),0);

	if ( strncmp(buf, "select", 6)==0 ) {
		pTable->select(buf);
	}
	else {
		if( sql_exec(buf, NULL, NULL) )
			pTable->redraw();
		else
			fl_alert("%s", sql_errmsg());
	}
}
void second_timer( void *pv )
{
	if ( pTable!=NULL ) {
		pTable->data_changed(true);
		pTable->redraw();
		if ( Fl::focus()!=pCmd ) {
			pCmd->value( pTable->select() );
			Fl::awake(pCmd);
		}
	}
	Fl::repeat_timeout(1, second_timer);
}
void table_change_callback(void *data, int type,
							const char *db_name,
							const char *tbl_name, sqlite3_int64 rowid)
{
	if ( !pTable->data_changed() )
		if ( strcmp(pTable->label(), tbl_name)==0 )
			pTable->data_changed(true);
}
void table_callback(Fl_Widget *w, void *data)
{
	char sql[256];
	sprintf( sql, "select * from %s", pMenu->text() );
	pTable->take_focus();
	pTable->select(sql);
	pMenu->redraw();
}

void table_open(const char *fn)
{
	if ( sql_open(fn) ) {
		char *tbl_names;
		sql_table("select name from sqlite_master where type='table'",
							&tbl_names);
		char *p0 = strchr(tbl_names, '\012');
		while ( p0!=NULL ) {
			char *p1 = strchr(++p0, '\012');
			if ( p1!=NULL ) *p1 = 0;

			char sql[1024];
			sprintf( sql, "Table/%s", p0 );
			pMenu->add(sql, 0, 	table_callback, NULL);
			p0 = p1;
		}
		pTableWin->copy_label(fn);
		sql_hook(table_change_callback, NULL);
	}
}
void dbopen_callback(Fl_Widget *w, void *data)
{
	const char *fn = file_chooser("Open database", "Database\t*.db",
								Fl_Native_File_Chooser::BROWSE_FILE);
	if ( fn!=NULL ) table_open(fn);
}
void dbsave_callback(Fl_Widget *w, void *data)
{
	const char *fn = file_chooser("Save database as", "Database\t*.db" );
	if ( fn!=NULL ) sql_save(fn);
}
void csvsave_callback(Fl_Widget *w, void *data)
{
	const char *fn = file_chooser("Save table to", "Spreadsheet\t*.csv" );
	if ( fn!=NULL )	pTable->save(fn);
}
void rowcopy_callback(Fl_Widget *w, void *data)
{
	pTable->copy_rows();
}
void rowpaste_callback(Fl_Widget *w, void *data)
{
	Fl::paste(*pTable, 1);
}
int httport;
void about_callback(Fl_Widget *w, void *data)
{
	fl_message(ABOUT_TABLE, httport);
}
int main(int argc, char **argv)
{
	httport= httpd_init();

	Fl::lock();
	Fl::scheme("gtk+");
	pTableWin = new Fl_Double_Window(1024, 640);
	{
		pMenu=new Fl_Sys_Menu_Bar(0, 0, pTableWin->w() ,MENUHEIGHT);
		pMenu->add("Database/Open...", 	"#o",	dbopen_callback, NULL);
		pMenu->add("Database/Save...", 	"#s",	dbsave_callback, NULL);
		pMenu->add("Database/About", 	"#a",	about_callback, NULL, FL_MENU_DIVIDER);
		pMenu->add("Table/Export...", 	0,		csvsave_callback, NULL, FL_MENU_DIVIDER);
		pMenu->add("Script/Run...", 	0, 		rowcopy_callback, 0);
		pMenu->textsize(18);
		pTable = new sqlTable(0, MENUHEIGHT, pTableWin->w(),
								pTableWin->h()-MENUHEIGHT-CMDHEIGHT, "" );
		pTable->end();
		pCmd = new Fl_Browser_Input(40, pTableWin->h()-CMDHEIGHT, 
									pTableWin->w()-40, CMDHEIGHT, "SQL:");
		pCmd->color(FL_GREEN, FL_DARK1);
		pCmd->box(FL_FLAT_BOX);
		pCmd->textsize(16);
		pCmd->when(FL_WHEN_ENTER_KEY_ALWAYS);
		pCmd->callback(cmd_callback);
		pTableWin->resizable(*pTable);
		pTableWin->end();
	}
#ifdef WIN32
    pTableWin->icon((char*)LoadIcon(fl_display, MAKEINTRESOURCE(128)));
#endif
	pTableWin->show();

	if ( argc>1 ) table_open(argv[1]);
	Fl::add_timeout(1, second_timer);
	while ( Fl::wait() ) {
		Fl_Widget *w = (Fl_Widget *)Fl::thread_message();
		if ( w!=NULL ) w->redraw();
	}

	httpd_exit();
	sql_close();
	return 0;
}
/***************************httpd**********************************************/
const char HEADER[]="HTTP/1.1 %s\
					\nServer: flTable-httpd\
					\nAccess-Control-Allow-Origin: *\
					\nContent-Type: text/plain\
					\nContent-length: %d\
					\nConnection: Keep-Alive\
					\nCache-Control: no-cache\n\n";	//max-age=1
void httpCGI( int http_s1, char *buf )
{
	for ( char *p=buf; *p; p++ ) if ( *p=='+' ) *p=' ';
	fl_decode_uri(buf);

	int replen = 0;
	char *reply=NULL;
	if ( strncmp(buf, "SQL=", 4)==0 )
		replen = sql_table( buf+4, &reply );

	int len = sprintf( buf, HEADER, "200 OK", replen );
	send( http_s1, buf, len, 0 );
	if ( replen>0 && reply!=NULL ) {
		send( http_s1, reply, replen, 0 );
		free(reply);
	}
}
void httpSession( int http_s1 )
{
	char buf[8192], *cmd=buf;
	int cmdlen;
	while ( (cmdlen=recv(http_s1,buf,4095,0))>0 ) {
		buf[cmdlen] = 0;
		if ( strncmp(buf, "GET /", 5)==0 ) {
			cmd = buf+5;
			char *p = strchr(cmd, ' ');
			if ( p!=NULL ) *p = 0;
			if ( *cmd=='?' )
				httpCGI(http_s1, ++cmd);
		}
		else if ( strncmp(buf, "POST", 4)==0 ) {
			char *p = strstr(buf, "Content-Length: ");
			if ( p==NULL ) continue;

			int len = atoi(p+16), l=0;
			if ( len>4095 ) { //request too big
				len = sprintf( buf, HEADER, "400 Fail", 27);
				send( http_s1, buf, len, 0 );
				send( http_s1, "Invalid request >4095 bytes", 27, 0);
				continue;
			}
			p = strstr(buf, "\015\012\015\012");
			if ( p!=NULL ) {
				cmd = p+4;
				l = strlen(cmd);
			}
			while ( l<len ){
				int rc = recv(http_s1, cmd+l, len-l, 0);
				if ( rc<0 ) break;
				l += rc;
			}
			cmd[l] = 0;
			if ( l!=len ) {	//request with invalid length
				len = sprintf( buf, HEADER, "400 Fail", 0);
				send( http_s1, buf, len, 0 );
				send( http_s1, "Incomplete request", 18, 0);
				continue;
			}
			httpCGI(http_s1, cmd);
		}
	}
	closesocket(http_s1);
 }
void httpd( int http_s0 )
{
	struct sockaddr_in cltaddr;
	socklen_t addrsize=sizeof(cltaddr);

	while ( true ) {
		int http_s1=accept( http_s0, (struct sockaddr*)&cltaddr, &addrsize );
		if ( http_s1==-1 ) break;
		std::thread httpThread(httpSession, http_s1);
		httpThread.detach();
	}
}
static int http_s0 = -1;
int httpd_init()
{
#ifdef WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,0), &wsadata);
#endif

	http_s0 = socket(AF_INET, SOCK_STREAM, 0);
	if ( http_s0 == -1 ) return -1;

	struct sockaddr_in svraddr;
	int addrsize=sizeof(svraddr);
	memset(&svraddr, 0, addrsize);
	svraddr.sin_family=AF_INET;
	svraddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	short port = 8079;
	int rc = -1;
	while ( rc==-1 && port<9080 ) {
		svraddr.sin_port=htons(++port);
		rc = bind(http_s0, (struct sockaddr*)&svraddr, addrsize);
	}
	if ( rc!=-1 ) {
		if ( listen(http_s0, 1)!=-1){
			std::thread httpThread(httpd, http_s0);
			httpThread.detach();
			return port;
		}
	}
	closesocket(http_s0);
	return -1;
}
void httpd_exit()
{
	closesocket(http_s0);
}
void log_print(const char *name, const char *msg, int len)
{
	fprintf(stderr, "***%s***%s\n", name, msg);
}
/****************************************************************************/
