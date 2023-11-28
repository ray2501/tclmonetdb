/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2016-2017 <Danilo Chang>
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monetdb/mapi.h"
#include "monetdbStubs.h"

#ifdef __cplusplus
}
#endif

/*
 * I think I need add MonetDBInitStubs definition here.
 */
MODULE_SCOPE Tcl_LoadHandle MonetDBInitStubs(Tcl_Interp *);

/*
 * Windows needs to know which symbols to export.  Unix does not.
 * BUILD_monetdb should be undefined for Unix.
 */
#ifdef BUILD_monetdb
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_monetdb */

typedef struct ThreadSpecificData {
    int initialized;                   /* initialization flag */
    Tcl_HashTable *monetdb_hashtblPtr; /* per thread hash table. */
    int stmt_count;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

TCL_DECLARE_MUTEX(myMutex);

/*
 * This struct is to record MonetDB database info,
 */
struct MonetDBDATA {
    Mapi dbh;
    Tcl_Interp *interp;
};

typedef struct MonetDBDATA MonetDBDATA;

/*
 * This struct is to record statement info,
 * Now only has one member, MapiHdl.
 */
struct MonetDBStmt {
    MapiHdl hdl;
};

typedef struct MonetDBStmt MonetDBStmt;

static Tcl_Mutex monetdbMutex;
static int monetdbRefCount = 0;
static Tcl_LoadHandle monetdbLoadHandle = NULL;

void MonetDB_Thread_Exit(ClientData clientdata) {
    /*
     * This extension records hash table info in ThreadSpecificData,
     * so we need delete it when we exit a thread.
     */
    Tcl_HashSearch search;
    Tcl_HashEntry *entry;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData(
        &dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->monetdb_hashtblPtr) {
        for (entry = Tcl_FirstHashEntry(tsdPtr->monetdb_hashtblPtr, &search);
             entry != NULL;
             entry = Tcl_FirstHashEntry(tsdPtr->monetdb_hashtblPtr, &search)) {
            // Try to delete entry
            Tcl_DeleteHashEntry(entry);
        }

        Tcl_DeleteHashTable(tsdPtr->monetdb_hashtblPtr);
        ckfree(tsdPtr->monetdb_hashtblPtr);
    }
}

static void DbDeleteCmd(void *db) {
    MonetDBDATA *pDb = (MonetDBDATA *)db;

    /*
     * Warning!!!
     * How to handle STAT_HANDLE command if users do not close correctly?
     */

    mapi_disconnect(pDb->dbh);
    mapi_destroy(pDb->dbh);
    pDb->dbh = 0;

    Tcl_Free((char *)pDb);
    pDb = 0;

    Tcl_MutexLock(&monetdbMutex);
    if (--monetdbRefCount == 0) {
        Tcl_FSUnloadFile(NULL, monetdbLoadHandle);
        monetdbLoadHandle = NULL;
    }
    Tcl_MutexUnlock(&monetdbMutex);
}

static int MONET_STMT(void *cd, Tcl_Interp *interp, int objc,
                      Tcl_Obj *const *objv) {
    MonetDBStmt *pStmt;
    Tcl_HashEntry *hashEntryPtr;
    int choice;
    int rc = TCL_OK;
    char *mHandle;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData(
        &dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->monetdb_hashtblPtr =
            (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->monetdb_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *STMT_strs[] = {
        "param_string",
        "param_numeric",
        "clear_params",
        "execute",
        "fetch_reset",
        "seek_row",
        "fetch_all_rows",
        "fetch_row",
        "fetch_row_list",
        "fetch_row_dict",
        "fetch_field",
        "get_field_count",
        "get_row_count",
        "rows_affected",
        "get_field_name",
        "get_field_type",
        "get_query",
        "get_columns",
        "close",
        0
    };

    enum STMT_enum {
        STMT_PARAM_STRING,
        STMT_PARAM_NUMERIC,
        STMT_CLEAR_PARAMS,
        STMT_EXECUTE,
        STMT_FETCH_RESET,
        STMT_SEEK_ROW,
        STMT_FETCH_ALL_ROWS,
        STMT_FETCH_ROW,
        STMT_FETCH_ROW_LIST,
        STMT_FETCH_ROW_DICT,
        STMT_FETCH_FIELD,
        STMT_GET_FIELD_COUNT,
        STMT_GET_ROW_COUNT,
        STMT_ROW_AFFECTED,
        STMT_GET_NAME,
        STMT_GET_TYPE,
        STMT_GET_QUERY,
        STMT_GET_COLUMNS,
        STMT_CLOSE
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], STMT_strs, "option", 0, &choice)) {
        return TCL_ERROR;
    }

    /*
     * Get back MonetDBStmt pointer from our hash table
     */
    mHandle = Tcl_GetStringFromObj(objv[0], 0);
    hashEntryPtr = Tcl_FindHashEntry(tsdPtr->monetdb_hashtblPtr, mHandle);
    if (!hashEntryPtr) {
        if (interp) {
            Tcl_Obj *resultObj = Tcl_GetObjResult(interp);
            Tcl_AppendStringsToObj(resultObj, "invalid handle ", mHandle,
                                   (char *)NULL);
        }

        return TCL_ERROR;
    }

    pStmt = Tcl_GetHashValue(hashEntryPtr);
    if (pStmt->hdl == NULL) {
        return TCL_ERROR;
    }

    switch ((enum STMT_enum)choice) {

    case STMT_PARAM_STRING: {
        char *ptr = NULL;
        int len = 0;
        int sql_type = 0;
        int field_number = 0;
        Tcl_Obj *return_obj;

        /* SQL type in MAPI header:
         * Problem: Could I use MAPI_AUTO directly?
         *
         * #define MAPI_AUTO	0
         * #define MAPI_TINY	1
         * #define MAPI_UTINY	2
         * #define MAPI_SHORT	3
         * #define MAPI_USHORT	4
         * #define MAPI_INT	5
         * #define MAPI_UINT	6
         * #define MAPI_LONG	7
         * #define MAPI_ULONG	8
         * #define MAPI_LONGLONG	9
         * #define MAPI_ULONGLONG	10
         * #define MAPI_CHAR	11
         * #define MAPI_VARCHAR	12
         * #define MAPI_FLOAT	13
         * #define MAPI_DOUBLE	14
         * #define MAPI_DATE	15
         * #define MAPI_TIME	16
         * #define MAPI_DATETIME	17
         * #define MAPI_NUMERIC	18
         */
        if (objc == 5) {
            if (Tcl_GetIntFromObj(interp, objv[2], &field_number) != TCL_OK) {
                return TCL_ERROR;
            }
            if (field_number < 0) {
                return TCL_ERROR;
            }

            if (Tcl_GetIntFromObj(interp, objv[3], &sql_type) != TCL_OK) {
                return TCL_ERROR;
            }
            if (sql_type < 0) {
                return TCL_ERROR;
            }

            ptr = Tcl_GetStringFromObj(objv[4], &len);

            /*
             * if sqltype is MAPI_AUTO, MAPI_CHAR or MAPI_VARCHAR
             * let user could use empty string.
             * It maybe not a good idea...
             */
            if (sql_type == MAPI_AUTO || sql_type == MAPI_CHAR ||
                sql_type == MAPI_VARCHAR) {
                if (!ptr) {
                    return TCL_ERROR;
                }
            } else {
                if (!ptr || len < 1) {
                    return TCL_ERROR;
                }
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "field_number SQL_type string");
            return TCL_ERROR;
        }

        if (mapi_param_string(pStmt->hdl, field_number, sql_type, ptr, NULL) !=
            MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_PARAM_NUMERIC: {
        int field_number = 0;
        int scale = 0;
        int precision = 0;
        char *ptr = NULL;
        int len = 0;
        Tcl_Obj *return_obj;

        if (objc == 6) {

            if (Tcl_GetIntFromObj(interp, objv[2], &field_number) != TCL_OK) {
                return TCL_ERROR;
            }
            if (field_number < 0) {
                return TCL_ERROR;
            }

            if (Tcl_GetIntFromObj(interp, objv[3], &scale) != TCL_OK) {
                return TCL_ERROR;
            }
            if (scale < 0) {
                return TCL_ERROR;
            }

            if (Tcl_GetIntFromObj(interp, objv[4], &precision) != TCL_OK) {
                return TCL_ERROR;
            }
            if (precision < 0) {
                return TCL_ERROR;
            }

            /*
             * OK, (void *) is a problem for me.
             * Here I use char * pointer to record data, is it OK?
             */
            ptr = Tcl_GetStringFromObj(objv[5], &len);

            if (!ptr || len < 1) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv,
                             "field_number scale precision numeric_string");
            return TCL_ERROR;
        }

        /*
         * Bind to a numeric variable, internally represented by MAPI_INT.
         */
        if (mapi_param_numeric(pStmt->hdl, field_number, scale, precision,
                               (void *)ptr) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_CLEAR_PARAMS: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_clear_params(pStmt->hdl) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_EXECUTE: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_execute(pStmt->hdl) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_FETCH_RESET: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_fetch_reset(pStmt->hdl) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_SEEK_ROW: {
        Tcl_WideInt rowne = 0;
        int whence = 0;
        Tcl_Obj *return_obj;

        if (objc == 4) {
            if (Tcl_GetWideIntFromObj(interp, objv[2], &rowne) != TCL_OK) {
                return TCL_ERROR;
            }
            if (rowne < 0) {
                return TCL_ERROR;
            }

            if (Tcl_GetIntFromObj(interp, objv[3], &whence) != TCL_OK) {
                return TCL_ERROR;
            }
            if (whence < 0 || whence > 2) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "rowne whence");
            return TCL_ERROR;
        }

        // #define MAPI_SEEK_SET	0
        // #define MAPI_SEEK_CUR	1
        // #define MAPI_SEEK_END	2
        if (mapi_seek_row(pStmt->hdl, rowne, whence) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_FETCH_ALL_ROWS: {
        int allrows = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        allrows = mapi_fetch_all_rows(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewWideIntObj(allrows));
        break;
    }

    case STMT_FETCH_ROW: {
        int result = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = mapi_fetch_row(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
        break;
    }

    case STMT_FETCH_ROW_LIST: {
        int result = 0;
        int field_count = 0;
        int field_number = 0;
        char *result_str;
        Tcl_Obj *pResultStr;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = mapi_fetch_row(pStmt->hdl);
        pResultStr = Tcl_NewListObj(0, NULL);
        if (result > 0) {
            field_count = mapi_get_field_count(pStmt->hdl);
            if (field_count > 0) {
                for (field_number = 0; field_number < field_count;
                     field_number++) {
                    result_str = mapi_fetch_field(pStmt->hdl, field_number);
                    if (!result_str) {
                        /*
                         * If a value is NULL -> setup to empty string
                         */
                        Tcl_ListObjAppendElement(interp, pResultStr,
                                                 Tcl_NewStringObj("", -1));
                    } else {
                        Tcl_ListObjAppendElement(
                            interp, pResultStr,
                            Tcl_NewStringObj(result_str, -1));
                    }
                }
            }
        }

        Tcl_SetObjResult(interp, pResultStr);
        break;
    }

    case STMT_FETCH_ROW_DICT: {
        int result = 0;
        int field_count = 0;
        int field_number = 0;
        char *result_name;
        char *result_str;
        Tcl_Obj *pResultStr;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = mapi_fetch_row(pStmt->hdl);
        pResultStr = Tcl_NewDictObj();
        if (result > 0) {
            field_count = mapi_get_field_count(pStmt->hdl);
            if (field_count > 0) {
                for (field_number = 0; field_number < field_count;
                     field_number++) {
                    result_name = mapi_get_name(pStmt->hdl, field_number);
                    result_str = mapi_fetch_field(pStmt->hdl, field_number);

                    if (!result_str) {
                        /*
                         * If a value is NULL, the returned dictionary for the
                         * row will not contain the corresponding key.
                         */
                        continue;
                    } else {
                        Tcl_DictObjPut(interp, pResultStr,
                                       Tcl_NewStringObj(result_name, -1),
                                       Tcl_NewStringObj(result_str, -1));
                    }
                }
            }
        }

        Tcl_SetObjResult(interp, pResultStr);
        break;
    }

    case STMT_FETCH_FIELD: {
        int field_number = 0;
        char *result_str;
        Tcl_Obj *return_obj;

        if (objc == 3) {
            if (Tcl_GetIntFromObj(interp, objv[2], &field_number) != TCL_OK) {
                return TCL_ERROR;
            }
            if (field_number < 0) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "field_number");
            return TCL_ERROR;
        }

        result_str = mapi_fetch_field(pStmt->hdl, field_number);
        if (!result_str) {
            /*
             * If a value is NULL -> setup to empty string
             */
            return_obj = Tcl_NewStringObj("", -1);
        } else {
            return_obj = Tcl_NewStringObj(result_str, -1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_GET_FIELD_COUNT: {
        int field_count = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        field_count = mapi_get_field_count(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewIntObj(field_count));
        break;
    }

    case STMT_GET_ROW_COUNT: {
        mapi_int64 row_count = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        row_count = mapi_get_row_count(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewWideIntObj(row_count));
        break;
    }

    case STMT_ROW_AFFECTED: {
        mapi_int64 row_count = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        row_count = mapi_rows_affected(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewWideIntObj(row_count));
        break;
    }

    case STMT_GET_NAME: {
        int field_number = 0;
        char *result_str;
        Tcl_Obj *return_obj;

        if (objc == 3) {
            if (Tcl_GetIntFromObj(interp, objv[2], &field_number) != TCL_OK) {
                return TCL_ERROR;
            }
            if (field_number < 0) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "field_number");
            return TCL_ERROR;
        }

        result_str = mapi_get_name(pStmt->hdl, field_number);

        return_obj = Tcl_NewStringObj(result_str, -1);
        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_GET_TYPE: {
        int field_number = 0;
        char *result_str;
        Tcl_Obj *return_obj;

        if (objc == 3) {
            if (Tcl_GetIntFromObj(interp, objv[2], &field_number) != TCL_OK) {
                return TCL_ERROR;
            }
            if (field_number < 0) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "field_number");
            return TCL_ERROR;
        }

        result_str = mapi_get_type(pStmt->hdl, field_number);

        return_obj = Tcl_NewStringObj(result_str, -1);
        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case STMT_GET_QUERY: {
        char *query = NULL;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        query = mapi_get_query(pStmt->hdl);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(query, -1));
        break;
    }

    case STMT_GET_COLUMNS: {
        int count = 0;
        int field_number = 0;
        char *result_str;
        Tcl_Obj *pResultStr;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        count = mapi_get_field_count(pStmt->hdl);
        pResultStr = Tcl_NewListObj(0, NULL);
        if (count > 0) {
            for (field_number = 0; field_number < count; field_number++) {
                result_str = mapi_get_name(pStmt->hdl, field_number);
                Tcl_ListObjAppendElement(interp, pResultStr,
                                         Tcl_NewStringObj(result_str, -1));
            }
        }

        Tcl_SetObjResult(interp, pResultStr);
        break;
    }

    case STMT_CLOSE: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_close_handle(pStmt->hdl) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        pStmt->hdl = 0;

        Tcl_Free((char *)pStmt);
        pStmt = 0;

        Tcl_MutexLock(&myMutex);
        if (hashEntryPtr)
            Tcl_DeleteHashEntry(hashEntryPtr);
        Tcl_MutexUnlock(&myMutex);

        Tcl_DeleteCommand(interp, Tcl_GetStringFromObj(objv[0], 0));
        Tcl_SetObjResult(interp, return_obj);

        break;
    }

    } /* End of the SWITCH statement */

    return rc;
}

static int DbObjCmd(void *cd, Tcl_Interp *interp, int objc,
                    Tcl_Obj *const *objv) {
    MonetDBDATA *pDb = (MonetDBDATA *)cd;
    int choice;
    int rc = TCL_OK;

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData(
        &dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->monetdb_hashtblPtr =
            (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->monetdb_hashtblPtr, TCL_STRING_KEYS);
    }

    static const char *DB_strs[] = {
        "reconnect",
        "is_connected",
        "ping",
        "monet_version",
        "getAutocommit",
        "setAutocommit",
        "query",
        "prepare",
        "close",
        0
    };

    enum DB_enum {
        DB_RECONNECT,
        DB_IS_CONNECTED,
        DB_PING,
        DB_MONET_VERSION,
        DB_GETAUTOCOMMIT,
        DB_SETAUTOCOMMIT,
        DB_QUERY,
        DB_PREPARE,
        DB_CLOSE,
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], DB_strs, "option", 0, &choice)) {
        return TCL_ERROR;
    }

    if (pDb->dbh == NULL) {
        return TCL_ERROR;
    }

    switch ((enum DB_enum)choice) {

    case DB_RECONNECT: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_reconnect(pDb->dbh) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case DB_IS_CONNECTED: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_is_connected(pDb->dbh) > 0) {
            return_obj = Tcl_NewBooleanObj(1);
        } else {
            return_obj = Tcl_NewBooleanObj(0);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case DB_PING: {
        Tcl_Obj *return_obj;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        if (mapi_ping(pDb->dbh) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case DB_MONET_VERSION: {
        char *errorStr = NULL;
        int result = 0;
        int field_count = 0;
        char *result_str = NULL;
        Tcl_Obj *return_obj = NULL;
        MapiHdl hdl;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        /*
         * Using mapi_query to query sys.environment table
         */
        hdl = mapi_query(
            pDb->dbh,
            "select value from sys.environment where name = 'monet_version'");
        if (hdl == NULL) {
            errorStr = mapi_error_str(pDb->dbh);
            Tcl_SetResult(interp, errorStr, NULL);

            return TCL_ERROR;
        }

        result = mapi_fetch_row(hdl);
        if (result > 0) {
            field_count = mapi_get_field_count(hdl);
            if (field_count > 0) {
                result_str = mapi_fetch_field(hdl, 0);
                if (!result_str) {
                    return_obj = Tcl_NewStringObj("", -1);
                } else {
                    return_obj = Tcl_NewStringObj(result_str, -1);
                }
            } else {
                Tcl_SetResult(interp, "No result can fetch!!!", NULL);

                mapi_close_handle(hdl);
                return TCL_ERROR;
            }
        } else {
            Tcl_SetResult(interp, "Result is empty!!!", NULL);

            mapi_close_handle(hdl);
            return TCL_ERROR;
        }

        mapi_close_handle(hdl);
        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case DB_GETAUTOCOMMIT: {
        int result = 0;

        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        result = mapi_get_autocommit(pDb->dbh);
        Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
        break;
    }

    case DB_SETAUTOCOMMIT: {
        int autocommit = 0;
        Tcl_Obj *return_obj;

        if (objc == 3) {
            if (Tcl_GetIntFromObj(interp, objv[2], &autocommit) != TCL_OK) {
                return TCL_ERROR;
            }

            if (autocommit < 0) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "autocommit");
            return TCL_ERROR;
        }

        if (mapi_setAutocommit(pDb->dbh, autocommit) != MOK) {
            return_obj = Tcl_NewBooleanObj(0);
            rc = TCL_ERROR;
        } else {
            return_obj = Tcl_NewBooleanObj(1);
        }

        Tcl_SetObjResult(interp, return_obj);
        break;
    }

    case DB_QUERY: {
        char *zQuery = NULL;
        char *errorStr = NULL;
        int len = 0;
        Tcl_HashEntry *newHashEntryPtr;
        char handleName[16 + TCL_INTEGER_SPACE];
        Tcl_Obj *pResultStr = NULL;
        MonetDBStmt *pStmt;
        int newvalue;

        if (objc == 3) {
            zQuery = Tcl_GetStringFromObj(objv[2], &len);

            if (!zQuery || len < 1) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "SQL_String");
            return TCL_ERROR;
        }

        pStmt = (MonetDBStmt *)Tcl_Alloc(sizeof(*pStmt));
        if (pStmt == 0) {
            Tcl_SetResult(interp, (char *)"malloc failed", TCL_STATIC);
            return TCL_ERROR;
        }

        pStmt->hdl = mapi_query(pDb->dbh, zQuery);
        if (pStmt->hdl == NULL) {
            errorStr = mapi_error_str(pDb->dbh);
            Tcl_SetResult(interp, errorStr, NULL);

            Tcl_Free((char *)pStmt);
            pStmt = 0;

            return TCL_ERROR;
        } else {
            Tcl_MutexLock(&myMutex);
            sprintf(handleName, "monetdb_stat%d", tsdPtr->stmt_count++);

            pResultStr = Tcl_NewStringObj(handleName, -1);

            newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->monetdb_hashtblPtr,
                                                  handleName, &newvalue);
            Tcl_SetHashValue(newHashEntryPtr, pStmt);
            Tcl_MutexUnlock(&myMutex);

            Tcl_CreateObjCommand(interp, handleName,
                                 (Tcl_ObjCmdProc *)MONET_STMT, (ClientData)NULL,
                                 (Tcl_CmdDeleteProc *)NULL);
        }

        Tcl_SetObjResult(interp, pResultStr);

        break;
    }

    case DB_PREPARE: {
        char *zQuery = NULL;
        char *errorStr = NULL;
        int len = 0;
        Tcl_HashEntry *newHashEntryPtr;
        char handleName[16 + TCL_INTEGER_SPACE];
        Tcl_Obj *pResultStr = NULL;
        MonetDBStmt *pStmt;
        int newvalue;

        if (objc == 3) {
            zQuery = Tcl_GetStringFromObj(objv[2], &len);

            if (!zQuery || len < 1) {
                return TCL_ERROR;
            }
        } else {
            Tcl_WrongNumArgs(interp, 2, objv, "SQL_String");
            return TCL_ERROR;
        }

        pStmt = (MonetDBStmt *)Tcl_Alloc(sizeof(*pStmt));
        if (pStmt == 0) {
            Tcl_SetResult(interp, (char *)"malloc failed", TCL_STATIC);
            return TCL_ERROR;
        }

        pStmt->hdl = mapi_prepare(pDb->dbh, zQuery);
        if (pStmt->hdl == NULL) {
            errorStr = mapi_error_str(pDb->dbh);
            Tcl_SetResult(interp, errorStr, NULL);

            Tcl_Free((char *)pStmt);
            pStmt = 0;

            return TCL_ERROR;
        } else {
            Tcl_MutexLock(&myMutex);
            sprintf(handleName, "monetdb_stat%d", tsdPtr->stmt_count++);

            pResultStr = Tcl_NewStringObj(handleName, -1);

            newHashEntryPtr = Tcl_CreateHashEntry(tsdPtr->monetdb_hashtblPtr,
                                                  handleName, &newvalue);
            Tcl_SetHashValue(newHashEntryPtr, pStmt);
            Tcl_MutexUnlock(&myMutex);

            Tcl_CreateObjCommand(interp, handleName,
                                 (Tcl_ObjCmdProc *)MONET_STMT, (ClientData)NULL,
                                 (Tcl_CmdDeleteProc *)NULL);
        }

        Tcl_SetObjResult(interp, pResultStr);

        break;
    }

    case DB_CLOSE: {
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, 0);
            return TCL_ERROR;
        }

        Tcl_DeleteCommand(interp, Tcl_GetStringFromObj(objv[0], 0));

        break;
    }

    } /* End of the SWITCH statement */

    return rc;
}

static int MONETDB_MAIN(void *cd, Tcl_Interp *interp, int objc,
                        Tcl_Obj *const *objv) {
    MonetDBDATA *p;
    const char *zArg;
    int i;
    const char *host = NULL;
    int port = 0;
    const char *username = NULL;
    const char *password = NULL;
    const char *dbname = NULL;

    if (objc < 2 || (objc & 1) != 0) {
        Tcl_WrongNumArgs(interp, 1, objv,
                         "HANDLE ?-host HOST? ?-port PORT? ?-user username? "
                         "?-passwd password? ?-dbname DBNAME? ");
        return TCL_ERROR;
    }

    for (i = 2; i + 1 < objc; i += 2) {
        zArg = Tcl_GetStringFromObj(objv[i], 0);

        if (strcmp(zArg, "-host") == 0) {
            host = Tcl_GetStringFromObj(objv[i + 1], 0);
        } else if (strcmp(zArg, "-port") == 0) {
            /*
             * Port number must be in range [0..65535], so check it.
             */
            if (Tcl_GetIntFromObj(interp, objv[i + 1], &port) != TCL_OK) {
                return TCL_ERROR;
            }

            if (port < 0 || port > 65535) {
                Tcl_AppendResult(interp,
                                 "port number must be in range [0..65535]",
                                 (char *)0);
                return TCL_ERROR;
            }
        } else if (strcmp(zArg, "-user") == 0) {
            username = Tcl_GetStringFromObj(objv[i + 1], 0);
        } else if (strcmp(zArg, "-passwd") == 0) {
            password = Tcl_GetStringFromObj(objv[i + 1], 0);
        } else if (strcmp(zArg, "-dbname") == 0) {
            dbname = Tcl_GetStringFromObj(objv[i + 1], 0);
        } else {
            Tcl_AppendResult(interp, "unknown option: ", zArg, (char *)0);
            return TCL_ERROR;
        }
    }

    /* Check our parameters */
    if (host == NULL) {
        host = "localhost";
    }

    if (port == 0) {
        port = 50000;
    }

    if (username == NULL) {
        username = "monetdb";
    }

    if (password == NULL) {
        password = "monetdb";
    }

    if (dbname == NULL) {
        dbname = "demo";
    }

    Tcl_MutexLock(&monetdbMutex);
    if (monetdbRefCount == 0) {
        if ((monetdbLoadHandle = MonetDBInitStubs(interp)) == NULL) {
            Tcl_MutexUnlock(&monetdbMutex);
            return TCL_ERROR;
        }
    }
    ++monetdbRefCount;
    Tcl_MutexUnlock(&monetdbMutex);

    p = (MonetDBDATA *)Tcl_Alloc(sizeof(*p));
    if (p == 0) {
        Tcl_SetResult(interp, (char *)"malloc failed", TCL_STATIC);
        return TCL_ERROR;
    }

    memset(p, 0, sizeof(*p));

    p->dbh = mapi_connect(host, port, username, password, "sql", dbname);

    if (mapi_error(p->dbh)) {
        p->dbh = 0;

        /*
         * It is a problem for me. Could I need to unload here?
         */
        Tcl_MutexLock(&monetdbMutex);
        if (--monetdbRefCount == 0) {
            Tcl_FSUnloadFile(NULL, monetdbLoadHandle);
            monetdbLoadHandle = NULL;
        }
        Tcl_MutexUnlock(&monetdbMutex);

        if (p)
            Tcl_Free((char *)p);
        Tcl_SetResult(interp, "Connect MonetDB fail", NULL);
        return TCL_ERROR;
    }

    p->interp = interp;

    zArg = Tcl_GetStringFromObj(objv[1], 0);
    Tcl_CreateObjCommand(interp, zArg, DbObjCmd, (char *)p, DbDeleteCmd);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Monetdb_Init --
 *
 *	Initialize the new package.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side effects:
 *	The Monetdb package is created.
 *
 *----------------------------------------------------------------------
 */

EXTERN int Monetdb_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *   Tcl_GetThreadData handles the auto-initialization of all data in
     *  the ThreadSpecificData to NULL at first time.
     */
    Tcl_MutexLock(&myMutex);
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData(
        &dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->initialized == 0) {
        tsdPtr->initialized = 1;
        tsdPtr->monetdb_hashtblPtr =
            (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(tsdPtr->monetdb_hashtblPtr, TCL_STRING_KEYS);

        tsdPtr->stmt_count = 0;
    }
    Tcl_MutexUnlock(&myMutex);

    /* Add a thread exit handler to delete hash table */
    Tcl_CreateThreadExitHandler(MonetDB_Thread_Exit, (ClientData)NULL);

    Tcl_CreateObjCommand(interp, "monetdb", (Tcl_ObjCmdProc *)MONETDB_MAIN,
                         (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
