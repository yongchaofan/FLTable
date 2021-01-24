HEADERS = src/sqlTable.h src/sql.h

TABLE_OBJS=obj/flTable.o obj/sqlTable.o obj/sql.o obj/Fl_Browser_Input.o\
		  ../sqlite3/sqlite3.o

INCLUDE = -I. -I../sqlite3
CFLAGS= -Os -std=c++11 ${shell fltk-config --cxxflags}
LDFLAGS = ${shell fltk-config --ldstaticflags} -lstdc++ -ldl -lpthread

all: FLTable

flTable: ${TABLE_OBJS} 
	cc -o "$@" ${TABLE_OBJS} ${LDFLAGS}
	
obj/%.o: src/%.cxx ${HEADERS}
	${CC} ${CFLAGS} ${INCLUDE} -c $< -o $@

clean:
	rm obj/*.o FLTable
