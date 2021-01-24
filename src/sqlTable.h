//
// "$Id: sqlTable.h 2446 2018-06-30 00:48:10 $"
//
// sqlTable -- A table widget which extends FLTK Fl_Table to
//             allow manipulation of data rows in SQLite3
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
#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table.H>
#include <vector>
#include <string>

#ifndef __SQL_TABLE_H__
#define __SQL_TABLE_H__

#define MAXCOLS 		16
#define ROW_FONTSIZE	14
#define HEADER_FONTSIZE 16
#define ROW_FONTFACE	FL_HELVETICA
#define HEADER_FONTFACE FL_HELVETICA_BOLD
//#define LABEL_FONTFACE	FL_COURIER_BOLD
typedef std::vector<std::string> Row;
class sqlTable : public Fl_Table {
private:
    int severityCol;
    int clearCol;
	int topRow;				//the top row currently displayed
    int viewRows;			//number of rows loaded and displayed
    int totalRows;			//number of rows are in the current query
    int dataChanged;		//true if loaded data need be refreshed
    int headerChanged;		//true if new select command is used
	int sort[MAXCOLS];
	Row header;
	std::vector<Row> _rowdata;

	std::string select_sql;
	std::string count_sql;
	std::string edit_sql;
	std::string where_clause;
	Fl_Input *edit_input;
	int edit_row, edit_col;
    int editing;			//true if a cell is being edited

protected:
    void draw_cell(TableContext context, int R=0, int C=0,
                   int X=0, int Y=0, int W=0, int H=0);
    void draw_sort_arrow(int X,int Y,int W,int H, int C);
	void add_col(const char *hdr);
    void col_dclick(int C);
	void cell_dclick(int R, int C);
	void cell_rclick(int R, int C);
	void start_edit(int R, int C);
	void done_edit();

public:
    sqlTable(int x, int y, int w, int h, const char *l=0);
	~sqlTable(){};
	int handle( int e );
	void draw();
    void resize (int X, int Y, int W, int H);

    void save(const char *fn);
    void insert_row();
    void update_row();
    void delete_rows();
    void copy_rows();
	void paste_rows();
	int  get_rows(char **pBuf);

    void insert(int argc, char **argv, char **col_names);
    void data_changed(int t) { dataChanged=t; }
	int data_changed() { return dataChanged; }
    void select(const char *cmd);
	const char *select() { return select_sql.c_str(); }
};

#endif //__SQL_TABLE_H__