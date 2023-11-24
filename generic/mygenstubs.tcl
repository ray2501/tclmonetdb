#!/usr/bin/tclsh

set variable [list tclsh ../tools/genExtStubs.tcl monetdbStubDefs.txt monetdbStubs.h monetdbStubInit.c]
exec {*}$variable