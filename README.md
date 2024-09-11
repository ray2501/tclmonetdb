tclmonetdb
=====

[MonetDB](https://www.monetdb.org/Home) is an open source column-oriented
database management system developed at the Centrum Wiskunde & Informatica
(CWI) in the Netherlands.

MonetDB excessively uses main memory for processing, but does not require
that all data fit in the available physical memory. To handle a dataset that
exceeds the available physical memory, MonetDB does not (only) rely on the
available swap space, but (also) uses memory-mapped files to exploit disk
storage beyond the swap space as virtual memory.

tclmonetdb is a Tcl extension by using
[MAPI library](https://www.monetdb.org/Documentation/SQLreference/Programming/MAPI)
to connect MonetDB. I write this extension to research MonetDB and MAPI.

[Tcl Database Connectivity (TDBC)](http://www.tcl.tk/man/tcl8.6/TdbcCmd/tdbc.htm)
is a common interface for Tcl programs to access SQL databases. Tclmonetdb's
TDBC interface (tdbc::monetdb) is based on tclmonetdb extension. I write
Tclmonetdb's TDBC interface to study TDBC interface.

This extension is using [Tcl_LoadFile](https://www.tcl.tk/man/tcl/TclLib/Load.htm) to load mapi library.
Before using this extension, please setup libmapi path environment variable.
Below is an example on Windows platform:

    set PATH=C:\Program Files\MonetDB\MonetDB5\bin;%PATH%

Below is an example on Linux platform to setup LD_LIBRARY_PATH environment
variable (if need):

    export LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH

This extension needs Tcl 8.6.

Notice:  
After Mar2018 SP1, it is impossible to just load mapi library 
on Linux or UNIX system. User needs patch mapilib and stream Makefile.ag 
and rebuild MonetDB.

For example,

    sed -i 's/WIN32?//g' clients/mapilib/Makefile.ag
    sed -i 's/@WIN32_TRUE@//g' clients/mapilib/Makefile.in
    sed -i 's/WIN32?//g' common/stream/Makefile.ag
    sed -i 's/@WIN32_TRUE@//g' common/stream/Makefile.in

The load library issue is from MonetDB itself, not this extension.

After Oct2020, MonetDB uses CMake to build its source code.
It is not necessary to do above changes.

Notice:
Since Aug2024, the file name of mapi library changed.
It is necessary to create a soft link by yourself
(or update monetdbStubDefs.txt LIBRARY name).

For example,

    sudo ln -s libmapi-11.51.3.so.27.0.0 libmapi.so.27


Related Extension
=====

Tcl users can use [TDBC-ODBC](https://www.tcl.tk/man/tcl/TdbcodbcCmd/tdbc_odbc.htm) bridge via MonetDB
[ODBC driver](https://www.monetdb.org/Documentation/SQLreference/Programming/ODBC)
to connect MonetDB.

Or you can try my other project [TDBCJDBC](https://github.com/ray2501/TDBCJDBC)
via MonetDB [JDBC driver](https://www.monetdb.org/Documentation/SQLreference/Programming/JDBC) to connect MonetDB.


License
=====

As of the Jul2015 version of MonetDB (released on Friday, August 28, 2015),
the license of MonetDB is the Mozilla Public License, version 2.0.

tclmonetdb is Licensed under Mozilla Public License, Version 2.0.


UNIX BUILD
=====

Building under most UNIX systems is easy, just run the configure script and 
then run make. For more information about the build process, see the 
tcl/unix/README file in the Tcl src dist. The following minimal example will 
install the extension in the /opt/tcl directory.

    $ cd tclmonetdb
    $ ./configure --prefix=/opt/tcl
    $ make
    $ make install

If you need setup directory containing tcl configuration (tclConfig.sh), 
below is an example:

    $ cd tclmonetdb
    $ ./configure --with-tcl=/opt/activetcl/lib
    $ make
    $ make install


WINDOWS BUILD
=====

The recommended method to build extensions under windows is to use the 
Msys + Mingw build process. This provides a Unix-style build while generating 
native Windows binaries. Using the Msys + Mingw build tools means that you 
can use the same configure script as per the Unix build to create a Makefile.

User can use MSYS/MinGW or MSYS2/MinGW-W64 to build tclmonetdb:

    $ ./configure --with-tcl=/c/tcl/lib
    $ make
    $ make install


Implement commands
=====

The interface to the MonetDB mapi library consists of single tcl command 
named `monetdb`. Once a MonetDB database connection is created, it can be 
controlled using methods of the HANDLE command.

monetdb HANDLE ?-host HOST? ?-port PORT? ?-user username? ?-passwd password? ?-dbname DBNAME?  
HANDLE reconnect  
HANDLE is_connected  
HANDLE ping  
HANDLE monet_version  
HANDLE getAutocommit  
HANDLE setAutocommit autocommit  
HANDLE query SQL_String   
HANDLE prepare SQL_String   
HANDLE close   
STMT_HANDLE param_string field_number SQL_type string  
STMT_HANDLE param_numeric field_number scale precision numeric_string  
STMT_HANDLE clear_params  
STMT_HANDLE execute  
STMT_HANDLE fetch_reset  
STMT_HANDLE seek_row rowne whence  
STMT_HANDLE fetch_all_rows  
STMT_HANDLE fetch_row  
STMT_HANDLE fetch_row_list  
STMT_HANDLE fetch_row_dict  
STMT_HANDLE fetch_field field_number   
STMT_HANDLE get_field_count  
STMT_HANDLE get_row_count  
STMT_HANDLE rows_affected  
STMT_HANDLE get_field_name field_number  
STMT_HANDLE get_field_type field_number  
STMT_HANDLE get_query  
STMT_HANDLE get_columns  
STMT_HANDLE close  

`monetdb` command options are used to make connection to MonetDB.
Below is the option default value (if user does not specify):

| Option            | Type      | Default                         | Additional description |
| :---------------- | :-------- | :------------------------------ | :--------------------- |
| host              | string    | localhost                       |
| port              | integer   | 50000                           |
| user              | string    | monetdb                         |
| passwd            | string    | monetdb                         |
| dbname            | string    | demo                            | Windows platform M5server.bat default name


`monet_version` method is to query `sys.environment` table and get
monet_version value. Notice: User needs have privileges to access
`sys.environment` table.

By default, autocommit mode is on. User can use `setAutocommit` command to 
switch autocommit mode off.

MonetDB/SQL supports a multi-statement transaction scheme marked by 
START TRANSACTION and closed with either COMMIT or ROLLBACK. The session 
variable AUTOCOMMIT can be set to true (default) if each SQL statement should 
be considered an independent transaction.

In the AUTOCOMMIT mode, you can use START TRANSACTION and COMMIT/ROLLBACK to 
indicate transactions containing multiple SQL statements. In this case, 
AUTOCOMMIT is automatically disabled by a START TRANSACTION, and reenabled 
by a COMMIT or ROLLBACK.

If AUTOCOMMIT mode is OFF, the START TRANSACTION is implicit, and you should
only use COMMIT/ROLLBACK.

Below is SQL type defined in MAPI.h, using in `param_string` command:

    #define MAPI_AUTO	0
    #define MAPI_TINY	1
    #define MAPI_UTINY	2
    #define MAPI_SHORT	3
    #define MAPI_USHORT	4
    #define MAPI_INT	5
    #define MAPI_UINT	6
    #define MAPI_LONG	7
    #define MAPI_ULONG	8
    #define MAPI_LONGLONG	9
    #define MAPI_ULONGLONG	10
    #define MAPI_CHAR	11
    #define MAPI_VARCHAR	12
    #define MAPI_FLOAT	13
    #define MAPI_DOUBLE	14
    #define MAPI_DATE	15
    #define MAPI_TIME	16
    #define MAPI_DATETIME	17
    #define MAPI_NUMERIC	18

Note: MonetDB doesn't support unsigned integer at the SQL level,
because the SQL:2003 only allows signed numerical data types.

MonetDB C types support MAPI_UTINY, MAPI_USHORT, MAPI_UINT, MAPI_ULONG and
MAPI_ULONGLONG, but doesn't support unsigned type at the SQL level.

## TDBC commands

tdbc::monetdb::connection create db host port username password dbname ?-option value...?

Connection to a MonetDB database is established by invoking `tdbc::monetdb::connection create`,
passing it the name to be used as a connection handle,
followed by a host name, port number, username, password and dbname.

The tdbc::monetdb::connection create object command supports the -encoding,
-isolation and -readonly option (only gets the default setting).

MonetDB driver for TDBC implements a statement object that represents a SQL statement in a database.
Instances of this object are created by executing the `prepare` or `preparecall` object
command on a database connection.

The `prepare` object command against the connection accepts arbitrary SQL code
to be executed against the database.

The `paramtype` object command allows the script to specify the type and direction of parameter
transmission of a variable in a statement.
Now MonetDB driver only specify the type work.

MonetDB driver paramtype accepts below type:  
auto, tinyint, smallint, integer, bigint, char, varchar, real, float, double,
date, time, timestamp, decimal and numeric

The `execute` object command executes the statement.


Examples
=====

## tclmonetdb example

Set and get autocommit -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo
    db setAutocommit 1
    set autocommit_flag [db getAutocommit]
    puts "Current autocommit status is $autocommit_flag"
    db close

List of tables -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db query "SELECT name FROM tables"]
    while {[$stmt fetch_row]} {
        puts "[$stmt fetch_field 0]"
    }

    $stmt close
    db close

List of tables (fetch_all_rows/seek_row method) -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db query "SELECT name FROM tables"]
    set count [$stmt fetch_all_rows]
    incr count -1

    for {set i $count} {$i >= 0} {incr i -1} {
        $stmt seek_row $i 0
        $stmt fetch_row
        puts "[$stmt fetch_field 0]"
    }

    $stmt close
    db close

A simple example -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo
    puts "Current connection status: [db is_connected]"

    set stmt [db query "CREATE TABLE emp (name VARCHAR(20), age INT)"]
    $stmt close

    set stmt [db query "INSERT INTO emp VALUES ('John', 23)"]
    puts "Affected rows: [$stmt rows_affected]"
    $stmt close

    set stmt [db query "INSERT INTO emp VALUES ('Mary', 22)"]
    puts "Affected rows: [$stmt rows_affected]"
    $stmt close

    set stmt [db query "SELECT * FROM emp"]

    puts "Current query: [$stmt get_query]"
    puts "Fetch rows number: [$stmt get_row_count]"
    puts "Fetch field number: [$stmt get_field_count]"

    set field_num [$stmt get_field_count]
    for {set i 0} {$i < $field_num} {incr i 1} {
        set field_name [$stmt get_field_name $i]
        set field_type [$stmt get_field_type $i]
        puts "Field $i name - $field_name, type - $field_type]"
    }

    while {[$stmt fetch_row]} {
        set result0 [$stmt fetch_field 0]
        set result1 [$stmt fetch_field 1]
        puts "$result0 is $result1"
    }
    $stmt close

    set stmt [db query "DROP TABLE emp"]
    $stmt close

    db close

Another simple example (update for MonetDB Mar2018 Release,
test `if not exists` support) -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db query "CREATE TABLE if not exists emp (name VARCHAR(20), age INT)"]
    $stmt close

    set stmt [db query "INSERT INTO emp VALUES ('John', 23)"]
    $stmt close

    set stmt [db query "INSERT INTO emp VALUES ('Mary', 22)"]
    $stmt close

    set stmt [db query "SELECT * FROM emp"]

    set count [$stmt get_row_count]
    for {set i 0} { $i < $count} {incr i 1} {
        puts "[$stmt fetch_row_list]"
    }
    $stmt close

    set stmt [db query "DROP TABLE if exists emp"]
    $stmt close

    db close

Another simple example -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db prepare "CREATE TABLE emp (name VARCHAR(20), age INT)"]
    $stmt execute
    $stmt close

    set stmt [db prepare "INSERT INTO emp VALUES ('John', 23)"]
    $stmt execute
    $stmt close

    set stmt [db prepare "INSERT INTO emp VALUES ('Mary', 22)"]
    $stmt execute
    $stmt close

    set stmt [db prepare "SELECT * FROM emp"]
    $stmt execute

    set count [$stmt get_row_count]
    for {set i 0} { $i < $count} {incr i 1} {
        set mydict [$stmt fetch_row_dict]

        set field_count [$stmt get_field_count]
        set columns [$stmt get_columns]
        for {set j 0} { $j < $field_count} {incr j 1} {
            puts "[dict get $mydict [lindex $columns $j]]"
        }    
    }

    set stmt [db prepare "DROP TABLE emp"]
    $stmt execute
    $stmt close

    db close

Prepare/Params example (for v0.2) -

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db prepare "CREATE TABLE emp (name VARCHAR(20), age INT)"]
    $stmt execute
    $stmt close

    set stmt [db prepare "INSERT INTO emp VALUES (?, ?)"]
    $stmt param_string 0 12 "John"
    $stmt param_string 1 5 23
    $stmt execute
    $stmt clear_params

    # test MAPI_AUTO, automatic type detection
    $stmt param_string 0 0 "Mary"
    $stmt param_string 1 0 22
    $stmt execute
    $stmt clear_params

    $stmt param_string 0 12 "Bahamut"
    $stmt param_string 1 5 19999
    $stmt execute
    $stmt clear_params
    $stmt close

    set stmt [db prepare "SELECT * FROM emp"]
    $stmt execute

    set count [$stmt get_row_count]
    for {set i 0} { $i < $count} {incr i 1} {
        set mydict [$stmt fetch_row_dict]

        set field_count [$stmt get_field_count]
        set columns [$stmt get_columns]
        for {set j 0} { $j < $field_count} {incr j 1} {
            puts "[dict get $mydict [lindex $columns $j]]"
        }
    }
    $stmt close

    set stmt [db prepare "DROP TABLE emp"]
    $stmt execute
    $stmt close

    db close

JSON is supported in MonetDB as a subtype over type VARCHAR,
which ensures that only valid JSON strings are added to the database.
MonetDB supports most of the JSON path expressions defined in
[JSONPath](http://goessner.net/articles/JsonPath/).

Below is an example:

    package require monetdb

    monetdb db -host localhost -port 50000 -user monetdb -passwd monetdb -dbname demo

    set stmt [db query "CREATE TABLE orders (
        id bigint AUTO_INCREMENT primary key,
        info json not null)"]
    $stmt close

    set stmt [db query {INSERT INTO orders (info) VALUES
        ('{ "customer": "Lily Bush", "items": {"product": "Diaper","qty": 24}}')}]
    $stmt close

    set stmt [db query {INSERT INTO orders (info) VALUES
        ('{ "customer": "Josh William", "items": {"product": "Toy Car","qty": 1}}')}]
    $stmt close

    set stmt [db query {INSERT INTO orders (info) VALUES
        ('{ "customer": "Mary Clark", "items": {"product": "Toy Train","qty": 2}}')}]
    $stmt close

    set stmt [db query {select json.filter(o.info, '$.items.product') as info from orders o}]
    set count [$stmt get_row_count]
    for {set i 0} { $i < $count} {incr i 1} {
        set mydict [$stmt fetch_row_dict]

        set field_count [$stmt get_field_count]
        set columns [$stmt get_columns]
        for {set j 0} { $j < $field_count} {incr j 1} {
            puts "[dict get $mydict [lindex $columns $j]]"
        }
    }
    $stmt close

    set stmt [db query "DROP TABLE orders"]
    $stmt close

    db close

## Tcl Database Connectivity (TDBC) interface

Below is an exmaple:

    package require tdbc::monetdb

    tdbc::monetdb::connection create db localhost 50000 monetdb monetdb demo

    set statement [db prepare \
        {create table person (id integer, name varchar(40) not null)}]
    $statement execute
    $statement close

    set statement [db prepare {insert into person values(1, 'leo')}]
    $statement execute
    $statement close

    set statement [db prepare {insert into person values(2, 'yui')}]
    $statement execute
    $statement close

    set statement [db prepare {SELECT * FROM person}]

    $statement foreach row {
        puts [dict get $row id]
        puts [dict get $row name]
    }

    $statement close

    set statement [db prepare {DROP TABLE person}]
    $statement execute
    $statement close
    db close

For blob data, it is necessary to encode these characters to a hex encoded
string when to add it to database. And when you get the blob data
from database, you get a hex encoded string, then you need to decode it to
characters.

Tcl can use `binary encode hex` and `binary decode hex` to encode/decode
hex string.

Below is an example to show the case.

    package require tdbc::monetdb

    tdbc::monetdb::connection create db localhost 50000 monetdb monetdb demo

    set statement [db prepare {DROP TABLE IF EXISTS record}]
    $statement execute
    $statement close

    set statement [db prepare \
        {create table record (name varchar(40) not null, memo blob, primary key(name))}]
    $statement execute
    $statement close

    db begintransaction

    set data [binary encode hex "Hello World!"]
    set statement [db prepare "insert into record values('Arthur Lin', '$data')"]
    $statement execute
    $statement close

    set statement [db prepare {insert into record values(:name, :memo)}]
    $statement paramtype name varchar
    $statement paramtype memo auto

    set name "George Liao"
    set memo [binary encode hex "He is a liar."]
    $statement execute

    set name "Gino Chang"
    set memo [binary encode hex "Thank you."]
    $statement execute

    set name "Orange Cat"
    set memo [binary encode hex "I miss you."]
    $statement execute

    db commit

    set statement [db prepare {SELECT * FROM record}]

    $statement foreach row {
        puts "name: [dict get $row name], memo: [binary decode hex [dict get $row memo]]"
    }

    $statement close

    set statement [db prepare {DROP TABLE record}]
    $statement execute
    $statement close

    db close

Another example:

    package require tdbc::monetdb

    tdbc::monetdb::connection create db localhost 50000 monetdb monetdb demo

    set statement [db prepare \
        {create table contact (name varchar(20) not null  UNIQUE, 
          email varchar(40) not null, primary key(name))}]
    $statement execute
    $statement close

    set statement [db prepare {insert into contact values(:name, :email)}]
    $statement paramtype name varchar
    $statement paramtype email varchar

    set name danilo
    set email danilo@test.com
    $statement execute

    set name scott
    set email scott@test.com
    $statement execute

    set myparams [dict create name arthur email arthur@example.com]
    $statement execute $myparams
    $statement close

    set statement [db prepare {SELECT * FROM contact}]
    $statement foreach row {
        puts [dict get $row name]
        puts [dict get $row email]
    }

    $statement close

    set statement [db prepare {DROP TABLE contact}]
    $statement execute
    $statement close
    db close

