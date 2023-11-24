/*
 * monetdbStubInit.c --
 *
 *	Stubs tables for the foreign MonetDB libraries so that
 *	Tcl extensions can use them without the linker's knowing about them.
 *
 * @CREATED@ 2018-03-30 00:45:37Z by genExtStubs.tcl from monetdbStubDefs.txt
 *
 *-----------------------------------------------------------------------------
 */

#include <tcl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "monetdb/mapi.h"
#include "monetdbStubs.h"

/*
 * Static data used in this file
 */

/*
 * ABI version numbers of the MonetDB API that we can cope with.
 */

static const char *const monetdbSuffixes[] = {
    "", ".26", ".14", ".13", ".12", ".11", ".10", ".9", ".8", NULL
};

/*
 * Names of the libraries that might contain the MonetDB API
 */

static const char *const monetdbStubLibNames[] = {
    /* @LIBNAMES@: DO NOT EDIT THESE NAMES */
    "libmapi", NULL
    /* @END@ */
};

/*
 * Names of the functions that we need from MonetDB
 */

static const char *const monetdbSymbolNames[] = {
    /* @SYMNAMES@: DO NOT EDIT THESE NAMES */
    "mapi_connect",
    "mapi_destroy",
    "mapi_disconnect",
    "mapi_reconnect",
    "mapi_ping",
    "mapi_error",
    "mapi_error_str",
    "mapi_get_autocommit",
    "mapi_setAutocommit",
    "mapi_prepare",
    "mapi_param_string",
    "mapi_param_numeric",
    "mapi_clear_params",
    "mapi_execute",
    "mapi_fetch_reset",
    "mapi_close_handle",
    "mapi_query",
    "mapi_seek_row",
    "mapi_fetch_all_rows",
    "mapi_fetch_row",
    "mapi_fetch_field",
    "mapi_get_field_count",
    "mapi_get_row_count",
    "mapi_rows_affected",
    "mapi_get_name",
    "mapi_get_type",
    "mapi_get_query",
    "mapi_is_connected",
    NULL
    /* @END@ */
};

/*
 * Table containing pointers to the functions named above.
 */

static monetdbStubDefs monetdbStubsTable;
monetdbStubDefs* monetdbStubs = &monetdbStubsTable;

/*
 *-----------------------------------------------------------------------------
 *
 * MonetDBInitStubs --
 *
 *	Initialize the Stubs table for the MonetDB API
 *
 * Results:
 *	Returns the handle to the loaded MonetDB client library, or NULL
 *	if the load is unsuccessful. Leaves an error message in the
 *	interpreter.
 *
 *-----------------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_LoadHandle
MonetDBInitStubs(Tcl_Interp* interp)
{
    int i, j;
    int status;			/* Status of Tcl library calls */
    Tcl_Obj* path;		/* Path name of a module to be loaded */
    Tcl_Obj* shlibext;		/* Extension to use for load modules */
    Tcl_LoadHandle handle = NULL;
				/* Handle to a load module */

    /* Determine the shared library extension */

    status = Tcl_EvalEx(interp, "::info sharedlibextension", -1,
			TCL_EVAL_GLOBAL);
    if (status != TCL_OK) return NULL;
    shlibext = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(shlibext);

    /* Walk the list of possible library names to find an MonetDB client */

    status = TCL_ERROR;
    for (i = 0; status == TCL_ERROR && monetdbStubLibNames[i] != NULL; ++i) {
	for (j = 0; status == TCL_ERROR && monetdbSuffixes[j] != NULL; ++j) {
	    path = Tcl_NewStringObj(monetdbStubLibNames[i], -1);
	    Tcl_AppendObjToObj(path, shlibext);
	    Tcl_AppendToObj(path, monetdbSuffixes[j], -1);
	    Tcl_IncrRefCount(path);

	    /* Try to load a client library and resolve symbols within it. */

	    Tcl_ResetResult(interp);
	    status = Tcl_LoadFile(interp, path, monetdbSymbolNames, 0,
			      (void*)monetdbStubs, &handle);
	    Tcl_DecrRefCount(path);
	}
    }

    /* 
     * Either we've successfully loaded a library (status == TCL_OK), 
     * or we've run out of library names (in which case status==TCL_ERROR
     * and the error message reflects the last unsuccessful load attempt).
     */
    Tcl_DecrRefCount(shlibext);
    if (status != TCL_OK) {
	return NULL;
    }
    return handle;
}
