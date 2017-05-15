#
# Tcl package index file
#
package ifneeded monetdb 0.9.5 \
    [list load [file join $dir libmonetdb0.9.5.so] monetdb]

package ifneeded tdbc::monetdb 0.9.5 \
    [list source [file join $dir tdbcmonetdb.tcl]]
