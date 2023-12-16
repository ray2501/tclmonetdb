#------------------------------------------------------------------------------
# tdbcmonetdb.tcl --
#
#      Tcl DataBase Connectivity MonetDB Driver
#      Class definitions and Tcl-level methods for the tdbc::monetdb bridge.
#
#------------------------------------------------------------------------------

package require tdbc
package require monetdb

package provide tdbc::monetdb 0.9.16


::namespace eval ::tdbc::monetdb {

    namespace export connection

}


#------------------------------------------------------------------------------
#
# tdbc::monetdb::connection --
#
#	Class representing a connection to a MonetDB database.
#
#-------------------------------------------------------------------------------

::oo::class create ::tdbc::monetdb::connection {

    superclass ::tdbc::connection

    constructor {host port username password databaseName args} {
        if {[llength $args] % 2 != 0} {
	    set cmd [lrange [info level 0] 0 end-[llength $args]]
	    return -code error \
		-errorcode {TDBC GENERAL_ERROR HY000 MonetDB WRONGNUMARGS} \
		"wrong # args, should be \"$cmd ?-option value?...\""
        }
        next

        if {[catch {monetdb [namespace current]::DB -host $host \
                      -port $port -user $username -passwd $password \
                      -dbname $databaseName} errorMsg]} {
            error $errorMsg
        }

        if {[llength $args] > 0} {
	    my configure {*}$args
        }
    }

    forward statementCreate ::tdbc::monetdb::statement create

    # No option to config (MonetDB MAPI not provide), return error.
    method configure args {
        if {[llength $args] == 0} {
            set result {-encoding utf-8}
            lappend result -isolation serializable
            lappend result -readonly 0
            return $result
        } elseif {[llength $args] == 1} {
	    set option [lindex $args 0]
	    switch -exact -- $option {
		-e - -en - -enc - -enco - -encod - -encodi - -encodin - 
		-encoding {
		    return utf-8
                }
		-i - -is - -iso - -isol - -isola - -isolat - -isolati -
		-isolatio - -isolation {
		    return serializable
                }
                -r - -re - -rea - -read - -reado - -readon - -readonl -
		-readonly {
		    return 0
                }
		default {
		    return -code error \
			-errorcode [list TDBC GENERAL_ERROR HY000 MonetDB \
					BADOPTION $option] \
			"bad option \"$option\": must be\
                         -encoding, -isolation or -readonly"
                }
            }
          } elseif {[llength $args] % 2 != 0} {
	    # Syntax error
	    set cmd [lrange [info level 0] 0 end-[llength $args]]
	    return -code error \
		-errorcode [list TDBC GENERAL_ERROR HY000 \
				MonetDB WRONGNUMARGS] \
		"wrong # args, should be \" $cmd ?-option value?...\""
         }

	# Set one or more options

	foreach {option value} $args {
	    switch -exact -- $option {
		-e - -en - -enc - -enco - -encod - -encodi - -encodin - 
		-encoding {
		    if {$value ne {utf-8}} {
			return -code error \
			    -errorcode [list TDBC FEATURE_NOT_SUPPORTED 0A000 \
					    MonetDB ENCODING] \
			    "-encoding not supported to setup."
		    }
		}
		-i - -is - -iso - -isol - -isola - -isolat - -isolati -
		-isolatio - -isolation {
		    if {$value ne {serializable}} {
			return -code error \
			    -errorcode [list TDBC FEATURE_NOT_SUPPORTED 0A000 \
					    MonetDB ISOLATION] \
			    "-isolation not supported to setup."
		    }
		}
		-r - -re - -rea - -read - -reado - -readon - -readonl -
		-readonly {
		    if {$value} {
			return -code error \
			    -errorcode [list TDBC FEATURE_NOT_SUPPORTED 0A000 \
					    MonetDB READONLY] \
			    "-readonly not supported to setup."
		    }
		}
		default {
		    return -code error \
			-errorcode [list TDBC GENERAL_ERROR HY000 \
					MonetDB BADOPTION $value] \
			"bad option \"$option\": must be\
                         -encoding, -isolation or -readonly"
		}
	    }
	}
        return
    }

    # invoke close method -> destroy our object
    method close {} {
        set mystats [my statements]
        foreach mystat $mystats {
            $mystat close
        }
        unset mystats

        [namespace current]::DB close
        next
    }

    method tables {{pattern %}} {
        set retval {}

        # Got table id, table name and table schema
        set stmt [[namespace current]::DB prepare "select t.id, \
                   s.name as schema_name, t.name \
                   from sys.tables as t, sys.schemas as s \
                   where t.name like '$pattern' and t.system = false and \
                   t.schema_id = s.id"]

        $stmt execute
        set count [$stmt get_row_count]
        for {set i 0} { $i < $count} {incr i 1} {
            set row [$stmt fetch_row_dict]
            dict set row name [string tolower [dict get $row name]]
            dict set retval [dict get $row name] $row
        }
        $stmt close

        return $retval
    }

    method columns {table {pattern %}} {
        set retval {}

        # Setup our pattern
        set pattern [string map [list \
                                     * {[*]} \
                                     ? {[?]} \
                                     \[ \\\[ \
                                     \] \\\[ \
                                     _ ? \
                                     % *] [string tolower $pattern]]
        set stmt [[namespace current]::DB prepare "select c.number as number, \
                   c.name as name, c.type as type, c.type_digits as digits, \
                   c.type_scale as scale, c.\"null\" as nullable from \
                   sys.columns c, sys.tables t where c.table_id = t.id and \
                   t.name = '$table' and t.system = false"]
        $stmt execute

        set count [$stmt get_row_count]
        for {set i 0} { $i < $count} {incr i 1} {
            set row [$stmt fetch_row_dict]
            dict set row name [string tolower [dict get $row name]]

            set column_name [dict get $row name]
            if {![string match $pattern $column_name]} {
                continue
            }

            dict set retval [dict get $row name] $row
        }
        $stmt close

        return $retval
    }

    method primarykeys {table} {
        set retval {}
        set stmt [[namespace current]::DB prepare "select t.name as tablename, \
                   s.name as schema_name, k.name as keyname from sys.keys k, \
                   sys.tables t, sys.schemas s where k.type = 0 and \
                   k.table_id = t.id and t.name = '$table' and \
                   s.id = t.schema_id"]

        # Got table name, table schema and key name
        $stmt execute
        set retval [dict create]
        set count [$stmt get_row_count]
        for {set i 0} { $i < $count} {incr i 1} {
            $stmt fetch_row
            set tablename [$stmt fetch_field 0]
            set schemaname [$stmt fetch_field 1]
            set keyname [$stmt fetch_field 2]

            if {[string compare $table $tablename]==0} {
                if { [catch {dict get $retval tableName}]} {
                   set vallist [list $tablename]
                } else {
                   if {$tablename ni $vallist} {
                       lappend vallist $tablename
                   }
                }
                dict set retval tableName $vallist

                if { [catch {set vallist2 [dict get $retval tableSchema]}]} {
                   set vallist2 [list $schemaname]
                } else {
                   if {$schemaname ni $vallist2} {
                       lappend vallist2 $schemaname
                   }
                }
                dict set retval tableSchema $vallist2

                if { [catch {set listval [dict get $retval columnName]}]} {
                   set listval [list $keyname]
                } else {
                   if {$keyname ni $listval} {
                       lappend listval $keyname
                   }
                }
                dict set retval columnName $listval
            }
        }
        $stmt close

        return $retval
    }

    method foreignkeys {args} {
        set length [llength $args]
        set retval {}
        set ftable ""

        if { $length != 2 || $length%2 != 0} {
            return -code error \
            -errorcode [list TDBC GENERAL_ERROR HY000 \
                    MonetDB WRONGNUMARGS] \
            "wrong # args: should be \
             [lrange [info level 0] 0 1] -foreign tableName"

            return $retval
        }

        # I only know how to list foreign keys of a table
        foreach {option table} $args {
            if {[string compare $option "-foreign"]==0} {
                set ftable $table
            } else {
                return $retval
            }
        }

        set sql "select t.name as tablename, \
                 s.name as schema_name, k.name as key from sys.keys k, \
                 sys.tables t, sys.schemas s where k.type=2 and \
                 t.id=k.table_id and t.name='$ftable' and s.id = t.schema_id"

        set stmt [[namespace current]::DB prepare $sql]

        # Got table name, table schema and key name
        $stmt execute
        set retval [dict create]
        set count [$stmt get_row_count]
        for {set i 0} { $i < $count} {incr i 1} {
            $stmt fetch_row
            set tablename [$stmt fetch_field 0]
            set schemaname [$stmt fetch_field 1]
            set keyname [$stmt fetch_field 2]

            if {[string compare $ftable $tablename]==0} {
                if { [catch {dict get $retval foreignTable}]} {
                   set vallist [list $tablename]
                } else {
                   if {$tablename ni $vallist} {
                       lappend vallist $tablename
                   }
                }
                dict set retval foreignTable $vallist

                if { [catch {set vallist2 [dict get $retval foreignSchema]}]} {
                   set vallist2 [list $schemaname]
                } else {
                   if {$schemaname ni $vallist2} {
                       lappend vallist2 $schemaname
                   }
                }
                dict set retval foreignSchema $vallist2

                if { [catch {set listval [dict get $retval foreignColumn]}]} {
                   set listval [list $keyname]
                } else {
                   if {$keyname ni $listval} {
                       lappend listval $keyname
                   }
                }
                dict set retval foreignColumn $listval
            }
        }
        $stmt close

        return $retval
    }

    # The 'prepareCall' method gives a portable interface to prepare
    # calls to stored procedures.  It delegates to 'prepare' to do the
    # actual work.
    method preparecall {call} {
        regexp {^[[:space:]]*(?:([A-Za-z_][A-Za-z_0-9]*)[[:space:]]*=)?(.*)} \
            $call -> varName rest
        if {$varName eq {}} {
            my prepare \\{$rest\\}
        } else {
            my prepare \\{:$varName=$rest\\}
        }
    }

    # The 'begintransaction' method launches a database transaction
    method begintransaction {} {
        set stmt [[namespace current]::DB prepare "START TRANSACTION"]
        $stmt execute
        $stmt close
    }

    # The 'commit' method commits a database transaction
    method commit {} {
        set stmt [[namespace current]::DB prepare "COMMIT"]
        $stmt execute
        $stmt close
    }

    # The 'rollback' method abandons a database transaction
    method rollback {} {
        set stmt [[namespace current]::DB prepare "ROLLBACK"]
        $stmt execute
        $stmt close
    }

    method prepare {sqlCode} {
        set result [next $sqlCode]
        return $result
    }

    method getDBhandle {} {
        return [namespace current]::DB
    }

}


#------------------------------------------------------------------------------
#
# tdbc::monetdb::statement --
#
#	The class 'tdbc::monetdb::statement' models one statement against a
#       database accessed through a MonetDB connection
#
#------------------------------------------------------------------------------

::oo::class create ::tdbc::monetdb::statement {

    superclass ::tdbc::statement

    variable Params db sql stmt

    constructor {connection sqlcode} {
        next
        set Params {}
        set db [$connection getDBhandle]
        set sql {}
        foreach token [::tdbc::tokenize $sqlcode] {

            # I have no idea how to get params meta by using MAPI here,
            # just give a default value.
            if {[string index $token 0] in {$ : @}} {
                dict set Params [string range $token 1 end] \
                    {type auto direction in}

                append sql "?"
                continue
            }

            append sql $token
        }

        set stmt [$db prepare $sql]
    }

    forward resultSetCreate ::tdbc::monetdb::resultset create

    method close {} {
        set mysets [my resultsets]
        foreach myset $mysets {
            $myset close
        }
        unset mysets

        $stmt close

        next
    }

    # The 'params' method returns descriptions of the parameters accepted
    # by the statement
    method params {} {
        return $Params
    }

    method paramtype args {
        set length [llength $args]

        if {$length < 2} {
            set cmd [lrange [info level 0] 0 end-[llength $args]]
            return -code error \
            -errorcode {TDBC GENERAL_ERROR HY000 MonetDB WRONGNUMARGS} \
            "wrong # args...\""
        }

        set parameter [lindex $args 0]
        if { [catch  {set value [dict get $Params $parameter]}] } {
            set cmd [lrange [info level 0] 0 end-[llength $args]]
            return -code error \
            -errorcode {TDBC GENERAL_ERROR HY000 MonetDB BADOPTION} \
            "wrong param...\""
        }

        set count 1
        if {$length > 1} {
            set direction [lindex $args $count]

            if {$direction in {in out inout}} {
                # I don't know how to setup direction, setup to in
                dict set value direction in
                incr count 1
            }
        }

        if {$length > $count} {
            set type [lindex $args $count]

            # Only accept these types
            if {$type in {auto tinyint smallint integer bigint char varchar \
                          real float double date time timestamp \
                          decimal numeric}} {
                dict set value type $type
            }
        }

        # Skip other parameters and setup
        dict set Params $parameter $value
    }

    method getStmthandle {} {
        return $stmt
    }

    method getSql {} {
        return $sql
    }

}


#------------------------------------------------------------------------------
#
# tdbc::monetdb::resultset --
#
#	The class 'tdbc::monetdb::resultset' models the result set that is
#	produced by executing a statement against a MonetDB database.
#
#------------------------------------------------------------------------------

::oo::class create ::tdbc::monetdb::resultset {

    superclass ::tdbc::resultset

    variable -set {*}{
        -stmt -sql -sqltypes -results -params -count -total -RowCount -columns
    }

    constructor {statement args} {
        next
    	set -stmt [$statement getStmthandle]
        set -params  [$statement params]
        set -sql [$statement getSql]
        set -results {}
        set -count 0
        set -total 0

        # for internal use
        set -sqltypes [dict create \
            auto 0 \
            tinyint 1 \
            smallint 3 \
            integer 5 \
            bigint 9 \
            char 11 \
            varchar 12 \
            real  13 \
            float 14 \
            double 14 \
            date 15 \
            time 16 \
            timestamp 17 \
            decimal 18 \
            numeric 18]

        if {[llength $args] == 0} {
            set keylist [dict keys ${-params}]
            set count 0

            ${-stmt} clear_params

            foreach mykey $keylist {

                if {[info exists ::$mykey] == 1} {
                    upvar 1 $mykey mykey1
                    set type [dict get ${-sqltypes} [dict get [dict get ${-params} $mykey] type] ]

                    catch {${-stmt} param_string $count $type $mykey1}
                }

                incr count 1
            }
            ${-stmt} execute
            set -RowCount [ ${-stmt} rows_affected ]
            set -total [ ${-stmt} get_row_count ]
        } elseif {[llength $args] == 1} {
            # If the dict parameter is supplied, it is searched for a key
            # whose name matches the name of the bound variable
            set -paramDict [lindex $args 0]

            set keylist [dict keys ${-params}]
            set count 0

            ${-stmt} clear_params

            foreach mykey $keylist {

                if {[catch {set bound [dict get ${-paramDict} $mykey]}]==0} {
                    set type [dict get ${-sqltypes} [dict get [dict get ${-params} $mykey] type] ]

                    catch {${-stmt} param_string $count $type $bound}
                }

                incr count 1
            }
            ${-stmt} execute
            set -RowCount [ ${-stmt} rows_affected ]
            set -total [ ${-stmt} get_row_count ]
        } else {
            return -code error \
            -errorcode [list TDBC GENERAL_ERROR HY000 \
                    MonetDB WRONGNUMARGS] \
            "wrong # args: should be\
                     [lrange [info level 0] 0 1] statement ?dictionary?"
        }
    }

    # Return a list of the columns
    method columns {} {
        set -columns [ ${-stmt} get_columns ]
        return ${-columns}
    }

    method nextresults {} {
        set have 0

        # Is this method enough?
        if { ${-total} > ${-count} } {
            set have 1
        } else {
            set have 0
        }

        return $have
    }

    method nextlist var {
        upvar 1 $var row
        set row {}

        variable mylist

        if { [catch {set mylist [ ${-stmt} fetch_row_list ]}] } {
            return 0
        }

        if {[llength $mylist] == 0} {
            # No exception but result is empty, update -count
            incr -count 1        
            return 0
        }

        set row $mylist
        incr -count 1

        return 1
    }

    method nextdict var {
        upvar 1 $var row
        set row {}

        variable mydict

        if { [catch {set mydict [ ${-stmt} fetch_row_dict ]}] } {
            return 0
        }

        if {[dict size $mydict] == 0} {
            # No exception but result is empty, update -count
            incr -count 1        
            return 0
        }

        set row $mydict
        incr -count 1

        return 1
    }

    # Return the number of rows affected by a statement
    method rowcount {} {
        return ${-RowCount}
    }

}
