//
// "$Id: sqlTable.cxx 14648 2018-08-18 00:48:10 $"
//
// sqlTable -- A table widget which extends FLTK Fl_Table to
//             allow manipulation of data rows in SQLite3
//
// Copyright 2017-2018 by Yongchao Fan.
//
// This library is free software distributed under GUN GPL 3.0,
// see the license at:
//
//	   https://github.com/zoudaokou/flTable/blob/master/LICENSE
//
// Please report all bugs and problems on the following page:
//
//	   https://github.com/zoudaokou/flTable/issues/new
//

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Menu.H>
#include "sqlTable.h"

typedef int (*sqlite3_callback)(
   void*,    /* Data provided in the 4th argument of sqlite3_exec()*/
   int,      /* The number of columns in row */
   char**,   /* An array of strings representing fields in the row */
   char**    /* An array of strings representing column names */
);
int sql_exec( const char *sql, sqlite3_callback sql_cb, void *data);
int sql_table(const char *sql, char **preply);
int sql_row(char *sql);

static int sql_callback(void *data, int argc, char **argv, char **col_names)
{
	sqlTable *pTable = (sqlTable *)data;
	pTable->insert(argc, argv, col_names);
	return 0;
}
void sqlTable::add_col( const char *hdr )
{
	header.push_back(std::string(hdr));
	int i = header.size()-1;
	if ( strcmp(hdr,"cleared" )==0 ) clearCol = i;
	if ( strcmp(hdr,"severity")==0 ) severityCol = i;
	{// Initialize column width to header width
		int w=0, h=0;
		fl_font(HEADER_FONTFACE, HEADER_FONTSIZE);
		fl_measure(hdr, w, h, 0);  // pixel width of header text
		col_width(i, w+24);
	}
}
void sqlTable::insert( int argc, char **argv, char **col_names )
{
	if ( headerChanged ) {
		headerChanged = false;
		header.clear();
		for ( int i=0; i<argc; i++ )
			add_col(col_names[i]);
		cols(argc);
	}

	Row newrow;
	newrow.clear();
	fl_font(ROW_FONTFACE, ROW_FONTSIZE);
	for ( int i=0; i<argc; i++ ) {
		const char *rowtext = argv[i]==NULL?"":argv[i];
		newrow.push_back(std::string(rowtext));
		int w=0, h=0;		//measure and set column width
		fl_measure(rowtext, w, h, 0);
		w += 24; if ( w>600 ) w=600;
		if ( w>col_width(i) ) col_width(i, w);
	}
	_rowdata.insert(_rowdata.begin(), newrow);
}

sqlTable::sqlTable(int x,int y,int w,int h,const char *l):Fl_Table(x,y,w,h,l)
{
	for ( int i=0; i<MAXCOLS; i++ ) sort[i]=0;
	col_header(1);
	col_resize(1);
	col_header_height(24);
	row_header(1);
	row_resize(1);
	row_height_all(16);			// height of all rows
	row_resize_min(16);
	labelfont(FL_COURIER_BOLD);
	labelsize(20);
	
	editing = false;
	edit_input = NULL;
	edit_row = edit_col = -1;
	severityCol = clearCol = -1;
	rows(0); cols(0);
	viewRows = h/24-2;

	select_sql = "select * from ";
	select_sql += l;
	select(select_sql.c_str());
}
void sqlTable::select(const char *cmd)
{
	std::string sql = cmd;
	std::size_t i = sql.find("from ");
	if ( i!=std::string::npos ) {
		count_sql="select COUNT(rowid) "+sql.substr(i);
		i += 5;
		select_sql = sql;
		std::size_t j = select_sql.find(" ", i);
		if ( j==std::string::npos ) j = select_sql.length();
		copy_label(select_sql.substr(i, j-i).c_str());
		headerChanged = dataChanged = true;
		redraw();
	}
	for ( int C=0; C<MAXCOLS; C++ ) sort[C] = 0;//clear sort arrows 8/22/2019
}
void sqlTable::draw_sort_arrow(int X,int Y,int W,int H, int C)
{
	int xlft = X+(W-6)-8;
	int xctr = X+(W-6)-4;
	int xrit = X+(W-6)-0;
	int ytop = Y+(H/2)-4;
	int ybot = Y+(H/2)+4;
	if ( sort[C]>0 ) {				// Engraved down arrow
		fl_color(FL_WHITE);
		fl_line(xrit, ytop, xctr, ybot);
		fl_color(41);					// dark gray
		fl_line(xlft, ytop, xrit, ytop);
		fl_line(xlft, ytop, xctr, ybot);
	}
	else {								// Engraved up arrow
		fl_color(FL_WHITE);
		fl_line(xrit, ybot, xctr, ytop);
		fl_line(xrit, ybot, xlft, ybot);
		fl_color(41);					// dark gray
		fl_line(xlft, ybot, xctr, ytop);
	}
}

// Handle drawing all cells in table
const Fl_Color cell_color[] = { FL_WHITE, FL_DARK1, fl_lighter(FL_BLUE), FL_YELLOW, FL_RED };
void sqlTable::draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H)
{
	switch ( context ) {
		case CONTEXT_COL_HEADER: {
			fl_push_clip(X,Y,W,H);
			{
				fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, color());
				fl_font(HEADER_FONTFACE, HEADER_FONTSIZE);
				fl_color(FL_BLACK);
				fl_draw(header[C].c_str(), X+4,Y,W,H,
						FL_ALIGN_LEFT|FL_ALIGN_BOTTOM, 0, 0);
				if ( sort[C]!=0 ) draw_sort_arrow(X,Y,W,H,C);
			}
			fl_pop_clip();
			break;
		}
		case CONTEXT_ROW_HEADER: {
			fl_push_clip(X,Y,W,H);
			{
				fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, color());
				fl_font(ROW_FONTFACE, ROW_FONTSIZE);
				fl_color(FL_BLACK);
				char hdr[8];
				sprintf(hdr, "% 4d",R+1);
				fl_draw(hdr, X,Y,W,H, FL_ALIGN_LEFT, 0, 0);
			}
			fl_pop_clip();
			break;
		}
		case CONTEXT_CELL: {
			int r = R-top_row();
			if ( r >= (int)_rowdata.size() ) break;
			if ( C >= (int)_rowdata[r].size() ) break;
			if ( r==edit_row && C==edit_col ) break;
			int icolor=0;
			const char *s = _rowdata[r][C].c_str();
			if ( C==severityCol && clearCol!=-1 ) {
				if ( _rowdata[r][clearCol]=="" ) {
					if ( strcmp(s, "critical")==0 ) icolor=4;
					else if ( strcmp(s, "major")==0 ) icolor=3;
					else if ( strcmp(s, "minor")==0 ) icolor=2;
					else if ( strcmp(s, "warning")==0 ) icolor=1;
				}
			}
			fl_push_clip(X,Y,W,H);
			{
				fl_color(is_selected(R,C) ? FL_CYAN : cell_color[icolor]);
				fl_rectf(X,Y,W,H);
				fl_font(ROW_FONTFACE, ROW_FONTSIZE);
				fl_color(FL_BLACK);
				fl_draw(s, X+4,Y+4,W,H, FL_ALIGN_LEFT|FL_ALIGN_TOP);
				fl_color(FL_LIGHT2); fl_rect(X,Y,W,H);
			}
			fl_pop_clip();
			break;
		}
		default: break;
	}
}
void sqlTable::draw()
{
	if ( !editing && ( dataChanged || topRow!=top_row()) ) {
		char sql[1024];
		dataChanged = false;
		topRow = top_row();
		strncpy(sql, count_sql.c_str(), 1023);
		if ( sql_row(sql) ) {
			totalRows = atoi(sql);
			rows(totalRows);
		}
		Fl::lock();
		for ( size_t i=0; i<_rowdata.size(); i++ )
			_rowdata[i].clear();
		_rowdata.clear();
		Fl::unlock();
		int offset = totalRows-viewRows-top_row();
		if ( offset<0 ) offset = 0;
		sprintf(sql, "%s limit %d offset %d", select_sql.c_str(),
							viewRows, offset);
		sql_exec(sql, sql_callback, this);
	}
	Fl_Table::draw();
}
void sqlTable::resize (int X, int Y, int W, int H)
{
	viewRows = H/24-2;
	dataChanged = true;
	Fl_Table::resize(X, Y, W, (viewRows+2)*24);
}
int sqlTable::handle( int e )
{
	switch( e ) {
		case FL_RELEASE: {
			take_focus();
			int R = callback_row();
			int C = callback_col();
			switch ( callback_context() ) {
			case CONTEXT_CELL:
				if ( Fl::event_clicks() ) {
					cell_dclick(R, C); return 1;
				}
				if ( Fl::event_button()==3 ) {
					cell_rclick(R, C); return 1;
				}
				break;
			case CONTEXT_COL_HEADER:
				if ( Fl::event_clicks() ) {
					col_dclick(C); return 1;
				}
			default:break;
			}
			break;
		}
		case FL_KEYBOARD: if ( edit_input==NULL ) break;
						  if ( !edit_input->visible() ) break;
			switch ( Fl::event_key() ) {
			case FL_Tab:
				done_edit();
				if ( ++edit_col==cols() ) edit_col=0;
				start_edit(edit_row, edit_col);
				redraw();
				return 1;
			case FL_Enter:
				done_edit();
				if ( edit_sql[0]=='i' ) {
					for ( int i=0; i<cols(); i++ )
						edit_sql = edit_sql + _rowdata[edit_row][i] + "\",\"";
					edit_sql = edit_sql.replace(edit_sql.size()-3,3,"\")");
				}
				else {
					edit_sql.replace(edit_sql.size()-1,1, " ");
					edit_sql = edit_sql + where_clause;
				}
				sql_exec(edit_sql.c_str(), NULL, NULL);
			case FL_Escape :
				edit_row = -1;
				edit_col = -1;
				edit_input->hide();
				remove(*edit_input);
				editing = false;
				change_cursor(FL_CURSOR_DEFAULT);
				redraw();
				return 1;
			}
			break;
		case FL_PASTE: paste_rows();
		case FL_DND_RELEASE:
		case FL_DND_DRAG:
		case FL_DND_ENTER:
		case FL_DND_LEAVE: return 1;
	}
	return(Fl_Table::handle(e));
}
void sqlTable::start_edit(int R, int C)
{
	if ( edit_input == NULL ) {
		edit_input = new Fl_Input(0,0,0,0);
		edit_input->hide();
		edit_input->maximum_size(128);
		edit_input->color(FL_YELLOW);
		end();
	}
	add(*edit_input);

	edit_row = R;
	edit_col = C;
	int X,Y,W,H;
	find_cell( CONTEXT_CELL, R+top_row(), C, X,Y,W,H );
	edit_input->resize(X,Y,W,H);
	edit_input->value(_rowdata[edit_row][edit_col].c_str());
	edit_input->show();
	edit_input->take_focus();
	edit_input->clear_changed();
	editing = true;
}
void sqlTable::done_edit()
{
	if ( edit_input->changed() ) {
		_rowdata[edit_row][edit_col]=edit_input->value();
		if ( edit_sql[0]=='u' ) {
			edit_sql = edit_sql + header[edit_col] + "=\"";
			edit_sql = edit_sql + edit_input->value() + "\",";
		}
	}
}
void sqlTable::insert_row()
{
	int row_top, col_left, row_bot, col_right;
	get_selection(row_top, col_left, row_bot, col_right);
	if ( row_top==-1 || col_left==-1 ) return;

	start_edit( row_top-top_row(), 0 );
	edit_sql = "insert into ";
	edit_sql = edit_sql + label() + " ('";
	for ( int i=0; i<cols(); i++ )
		edit_sql = edit_sql + header[i] + "','";
	edit_sql = edit_sql.replace(edit_sql.size()-3, 3, "') values (\"");
}
void sqlTable::update_row()
{
	int row_top, col_left, row_bot, col_right;
	get_selection(row_top, col_left, row_bot, col_right);
	if ( row_top==-1 || col_left==-1 ) return;

	int r = row_top-top_row();
	start_edit( r, col_left );
	edit_sql = "update ";
	edit_sql = edit_sql + label() + " set ";
	where_clause = " where ";
	for ( int i=0; i<cols(); i++ )
		where_clause = where_clause + header[i]+"='"+_rowdata[r][i]+"' and ";
	where_clause = where_clause.replace(where_clause.size()-5,5,"");
}
void sqlTable::delete_rows()
{
	int row_top, col_left, row_bot, col_right;
	get_selection(row_top, col_left, row_bot, col_right);
	if ( row_top==-1 || col_left==-1 ) return;
	if ( row_top<topRow || row_bot-row_top>_rowdata.size() ) {
		fl_alert("delete displayed rows only");
		return;
	}

	for ( int r=row_top-topRow; r<=row_bot-topRow; r++ ) {
		edit_sql = "delete from ";
		edit_sql = edit_sql + label() + " where ";
		for ( int i=col_left; i<=col_right; i++ )
			edit_sql = edit_sql + header[i]+"='"+_rowdata[r][i]+"' and ";
		edit_sql = edit_sql.replace(edit_sql.size()-5,5,"");
		sql_exec(edit_sql.c_str(), NULL, NULL);
	}
	redraw();
}
int sqlTable::get_rows(char **pBuf)
{
	int row_top, col_left, row_bot, col_right;
	get_selection(row_top, col_left, row_bot, col_right);
	if ( row_top==-1 || col_left==-1 ) return 0;
	char limit[256];
	sprintf(limit, "%d offset %d", (row_bot-row_top+1), (totalRows-row_bot-1) );
	std::size_t from = select_sql.find(" from ");
	std::string copy_sql = "select ";
	for ( int C=col_left; C<col_right; C++ ) copy_sql += header[C] + ",";
	copy_sql += header[col_right] + select_sql.substr(from) + " limit " + limit;

	return sql_table(copy_sql.c_str(), pBuf);
}
void sqlTable::copy_rows()
{
	char *buf;
	int len = get_rows(&buf);
	Fl::copy(buf, len, 1);
	free(buf);
}
void sqlTable::paste_rows()
{
	char txt[1024];
	const char *p1 = strchr(Fl::event_text(), 0x0a);
	if ( p1==NULL ) return;
	const char *p0 = Fl::event_text();
	int l = p1-p0;
	strncpy( txt, p0, l); txt[l]=0;
	for ( char *p2=txt; *p2; p2++ ) if ( *p2=='\t' ) *p2=',';
	edit_sql = "insert or replace into ";
	edit_sql = edit_sql + label() + " (";
	edit_sql = edit_sql + txt + ") values (\"";
	int header_len = edit_sql.size();

	while ( p1!=NULL ) {
		p0 += l+1;
		p1 = strchr(p0, 0x0a);
		if ( p1!=NULL )
			l = p1-p0;
		else
			l = strlen(p0);
		strncpy(txt, p0, l); txt[l]=0;
		edit_sql = edit_sql + txt + "\")";
		int i = edit_sql.find("\t");
		while ( i!=-1 ){
			edit_sql.replace(i,1,"\",\"");
			i = edit_sql.find("\t");
		}
		sql_exec(edit_sql.c_str(), NULL, NULL);
		edit_sql.erase( header_len );
	}
	redraw();
}
void sqlTable::col_dclick(int COL)	//dclick on col header to change sorting
{
	if ( sort[COL]==0 ) {			//no sort, change to ASC
		int sort_order=1;
		for ( int i=0; i<MAXCOLS; i++ ) {
			if ( sort[i]==sort_order ) sort_order = 1+sort[i];
			if ( sort[i]==-sort_order ) sort_order = 1-sort[i];
		}
		sort[COL]=sort_order;
	}
	else if ( sort[COL]>0 ) {		//ASC sort, change to DESC
		sort[COL]=-sort[COL];
	}
	else if ( sort[COL]<0 ) {		//DESC sort, change to no sort
		int sort_order=-sort[COL];
		for ( int i=0; i<MAXCOLS; i++ ) {
			if ( sort[i]>sort_order ) sort[i]-=1;
			if ( sort[i]<-sort_order ) sort[i]+=1;
		}
		sort[COL]=0;
	}

	std::size_t i = select_sql.find(" order by");
	if ( i==std::string::npos ) i = select_sql.find(" limit");
	if ( i!=std::string::npos ) select_sql.erase(i);
	for ( int k=1; k<7; k++ ) {
		for ( int j=0; j<MAXCOLS; j++ ) {
			if ( sort[j]==k || sort[j]==-k ) {
				select_sql = select_sql + ((k==1)?" order by ":", ");
				select_sql = select_sql + header[j];
				if ( sort[j]<0 ) select_sql = select_sql + " DESC";
			}
		}
	}
	i = select_sql.find("from ");
	count_sql="select COUNT(rowid) "+select_sql.substr(i);
	dataChanged=true;
	redraw();
}
void sqlTable::cell_dclick(int R, int C)	//double click on cell to filter
{
	std::string order_by = "";
	std::size_t i = select_sql.find(" order by");
	if ( i==std::string::npos ) i = select_sql.find(" limit");
	if ( i!=std::string::npos ) {
		order_by = select_sql.substr(i);
		select_sql.erase(i);
	}
	int r = R-top_row();
	std::string filter = " ";
	filter = filter + header[C] + "='" + _rowdata[r][C] + "'";
	int i2 = select_sql.find(filter + " and");
	int i1 = select_sql.find(" and" + filter);
	int i0 = select_sql.find(" where" + filter);
	if ( i2!=-1) select_sql.erase(i2, filter.size()+4);
	else if ( i1!=-1 ) select_sql.erase(i1, filter.size()+4);
	else if ( i0!=-1 ) select_sql.erase(i0, filter.size()+6);
	else {
		if ( select_sql.find(" where ")!=std::string::npos )
			select_sql = select_sql + " and" + filter;
		else
			select_sql = select_sql + " where" + filter;
	}
	select_sql += order_by;
	i = select_sql.find("from ");
	count_sql="select COUNT(rowid) "+select_sql.substr(i);
	dataChanged=true;
	redraw();
}
void sqlTable::cell_rclick(int R, int C)	//cell right click menu
{
	const Fl_Menu_Item *m;
	Fl_Menu_Item rclick_menu[]={{"Copy"},{"Paste"},
								{"Insert"},{"Delete"},{"Update"},{0}};
	if ( !is_selected(R, C) ) set_selection(R, 0, R, cols()-1);
	m=rclick_menu->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
	if ( m ) {
		const char *sel = m->label();
		switch ( *sel ) {
		case 'I': insert_row(); break;
		case 'D': delete_rows(); break;
		case 'U': update_row(); break;
		case 'C': copy_rows(); break;
		case 'P': Fl::paste( *this, 1 ); break;
		}
	}
}
void sqlTable::save( const char *fn )
{
	FILE *fp = fopen( fn, "w+" );
	if ( fp!=NULL  ){
		for ( int i=0; i<(int)_rowdata[0].size()-1; i++ )
			fprintf( fp, "%s, ", header[i].c_str() );
		for ( int i=0; i<(int)_rowdata.size(); i++ ) {
			fprintf( fp, "\n" );
			for ( int j=0; j<(int)_rowdata[i].size()-1; j++ )
				fprintf( fp, "%s, ", _rowdata[i][j].c_str() );
		}
		fclose(fp);
	}
}
