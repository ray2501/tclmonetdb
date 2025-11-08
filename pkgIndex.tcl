# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded monetdb 0.10.1 \
	    [list load [file join $dir libtcl9monetdb0.10.1.so] [string totitle monetdb]]
} else {
    package ifneeded monetdb 0.10.1 \
	    [list load [file join $dir libmonetdb0.10.1.so] [string totitle monetdb]]
}

package ifneeded tdbc::monetdb 0.10.1 \
    [list source [file join $dir tdbcmonetdb.tcl]]
