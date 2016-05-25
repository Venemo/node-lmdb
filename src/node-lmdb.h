
// This file is part of node-lmdb, the Node.js binding for lmdb
// Copyright (c) 2013 Timur Kristóf
// Licensed to you under the terms of the MIT license
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef NODE_LMDB_H
#define NODE_LMDB_H

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <uv.h>
#include "../libraries/liblmdb/lmdb.h"

#define NanReturnThis() return info.GetReturnValue().Set(info.This())

#define TYPE_UNKNOWN 0
#define TYPE_BINARY 1
#define TYPE_STRING 2
#define TYPE_NUMBER 3
#define TYPE_BOOLEAN 4
#define TYPE_OBJECT 5

using namespace v8;
using namespace node;

// Exports misc stuff to the module
void setupExportMisc(Handle<Object> exports);

// Helper callback
typedef void (*argtokey_callback_t)(MDB_val &key);

void consoleLog(Local<Value> val);
void consoleLog(const char *msg);
void consoleLogN(int n);
void setFlagFromValue(int *flags, int flag, const char *name, bool defaultValue, Local<Object> options);
argtokey_callback_t argToKey(const Local<Value> &val, MDB_val &key, bool keyIsUint32);
Local<Value> keyToHandle(MDB_val &key, bool keyIsUint32);
Local<Value> valToString(MDB_val &data);
Local<Value> valToBinary(MDB_val &data);
Local<Value> valToNumber(MDB_val &data);
Local<Value> valToBoolean(MDB_val &data);
Local<Value> valToVal(MDB_val &data);

/*
    `Env`
    Represents a database environment.
    (Wrapper for `MDB_env`)
*/
class EnvWrap : public Nan::ObjectWrap {
private:
    // The wrapped object
    MDB_env *env;
    // Constructor for TxnWrap
    static Nan::Persistent<Function> txnCtor;
    // Constructor for DbiWrap
    static Nan::Persistent<Function> dbiCtor;

    friend class TxnWrap;
    friend class DbiWrap;

public:
    EnvWrap();
    ~EnvWrap();

    // Sets up exports for the Env constructor
    static void setupExports(Handle<Object> exports);

    /*
        Constructor of the database environment. You need to `open()` it before you can use it.
        (Wrapper for `mdb_env_create`)
    */
    static NAN_METHOD(ctor);

    /*
        Opens the database environment with the specified options. The options will be used to configure the environment before opening it.
        (Wrapper for `mdb_env_open`)

        Parameters:

        * Options object that contains possible configuration options.

        Possible options are:

        * maxDbs: the maximum number of named databases you can have in the environment (default is 1)
        * maxReaders: the maximum number of concurrent readers of the environment (default is 126)
        * mapSize: maximal size of the memory map (the full environment) in bytes (default is 10485760 bytes)
        * path: path to the database environment
    */
    static NAN_METHOD(open);

    /*
        Closes the database environment.
        (Wrapper for `mdb_env_close`)
    */
    static NAN_METHOD(close);

    /*
        Starts a new transaction in the environment.
        (Wrapper for `mdb_txn_begin`)

        Parameters:

        * Options object that contains possible configuration options.

        Possible options are:

        * readOnly: if true, the transaction is read-only
    */
    static NAN_METHOD(beginTxn);

    /*
        Opens a database in the environment.
        (Wrapper for `mdb_dbi_open`)

        Parameters:

        * Options object that contains possible configuration options.

        Possible options are:

        * name: the name of the database (or null to use the unnamed database)
        * create: if true, the database will be created if it doesn't exist
        * keyIsUint32: if true, keys are treated as 32-bit unsigned integers
        * dupSort: if true, the database can hold multiple items with the same key
        * reverseKey: keys are strings to be compared in reverse order
        * dupFixed: if dupSort is true, indicates that the data items are all the same size
        * integerDup: duplicate data items are also integers, and should be sorted as such
        * reverseDup: duplicate data items should be compared as strings in reverse order
    */
    static NAN_METHOD(openDbi);

    /*
        Flushes all data to the disk asynchronously.
        (Asynchronous wrapper for `mdb_env_sync`)

        Parameters:

        * Callback to be executed after the sync is complete.
    */
    static NAN_METHOD(sync);
};

/*
    `Txn`
    Represents a transaction running on a database environment.
    (Wrapper for `MDB_txn`)
*/
class TxnWrap : public Nan::ObjectWrap {
private:
    // The wrapped object
    MDB_txn *txn;

    // Reference to the MDB_env of the wrapped MDB_txn
    MDB_env *env;

    EnvWrap *ew;

    friend class CursorWrap;
    friend class DbiWrap;

public:
    TxnWrap(MDB_env *env, MDB_txn *txn);
    ~TxnWrap();

    // Constructor (not exposed)
    static NAN_METHOD(ctor);

    // Helper for all the get methods (not exposed)
    static Nan::NAN_METHOD_RETURN_TYPE getCommon(Nan::NAN_METHOD_ARGS_TYPE info, Local<Value> (*successFunc)(MDB_val&));

    // Helper for all the put methods (not exposed)
    static Nan::NAN_METHOD_RETURN_TYPE putCommon(Nan::NAN_METHOD_ARGS_TYPE info, void (*fillFunc)(Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&), void (*freeFunc)(MDB_val&));

    /*
        Commits the transaction.
        (Wrapper for `mdb_txn_commit`)
    */
    static NAN_METHOD(commit);

    /*
        Aborts the transaction.
        (Wrapper for `mdb_txn_abort`)
    */
    static NAN_METHOD(abort);

    /*
        Aborts a read-only transaction but makes it renewable with `renew`.
        (Wrapper for `mdb_txn_reset`)
    */
    static NAN_METHOD(reset);

    /*
        Renews a read-only transaction after it has been reset.
        (Wrapper for `mdb_txn_renew`)
    */
    static NAN_METHOD(renew);

    /*
        Gets string data (JavaScript string type) associated with the given key from a database. You need to open a database in the environment to use this.
        This method is zero-copy and the return value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getString);

    /*
        Gets binary data (Node.js Buffer) associated with the given key from a database. You need to open a database in the environment to use this.
        This method is zero-copy and the return value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getBinary);

    /*
        Gets number data (JavaScript number type) associated with the given key from a database. You need to open a database in the environment to use this.
        This method will copy the value out of the database.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getNumber);

    /*
        Gets boolean data (JavaScript boolean type) associated with the given key from a database. You need to open a database in the environment to use this.
        This method will copy the value out of the database.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getBoolean);

    /*
        Gets data associated with the given key from a database. You need to open a database in the environment to use this.
        When data is Number or Boolean this method will copy the value out of the database.
        When data is String or Buffer this method is zero-copy and the return value can only be used until the next put
        operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(get);

    /*
        Puts string data (JavaScript string type) into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putString);

    /*
        Puts object data into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putObject);

    /*
        Puts binary data (Node.js Buffer) into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putBinary);

    /*
        Puts number data (JavaScript number type) into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putNumber);

    /*
        Puts boolean data (JavaScript boolean type) into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putBoolean);

    /*
        Puts data into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(put);

    /*
        Deletes data with the given key from the database.
        (Wrapper for `mdb_del`)

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
    */
    static NAN_METHOD(del);
};

/*
    `Dbi`
    Represents a database instance in an environment.
    (Wrapper for `MDB_dbi`)
*/
class DbiWrap : public Nan::ObjectWrap {
private:
    // Stores whether keys should be treated as uint32_t
    bool keyIsUint32;
    // The wrapped object
    MDB_dbi dbi;
    // Reference to the MDB_env of the wrapped MDB_dbi
    MDB_env *env;

    EnvWrap *ew;

    friend class TxnWrap;
    friend class CursorWrap;

public:
    DbiWrap(MDB_env *env, MDB_dbi dbi);
    ~DbiWrap();

    // Constructor (not exposed)
    static NAN_METHOD(ctor);

    /*
        Closes the database instance.
        Wrapper for `mdb_dbi_close`)
    */
    static NAN_METHOD(close);

    /*
        Drops the database instance, either deleting it completely (default) or just freeing its pages.

        Parameters:

        * Options object that contains possible configuration options.

        Possible options are:

        * justFreePages - indicates that the database pages need to be freed but the database shouldn't be deleted

    */
    static NAN_METHOD(drop);

    static NAN_METHOD(stat);

    static NAN_GETTER(getEnv);
};

/*
    `Cursor`
    Represents a cursor instance that is assigned to a transaction and a database instance
    (Wrapper for `MDB_cursor`)
*/
class CursorWrap : public Nan::ObjectWrap {
private:
    // The wrapped object
    MDB_cursor *cursor;
    // Stores whether keys should be treated as uint32_t
    bool keyIsUint32;
    // Key/data pair where the cursor is at
    MDB_val key, data;
    DbiWrap *dw;
    TxnWrap *tw;

public:
    CursorWrap(MDB_cursor *cursor);
    ~CursorWrap();

    // Sets up exports for the Cursor constructor
    static void setupExports(Handle<Object> exports);

    /*
        Opens a new cursor for the specified transaction and database instance.
        (Wrapper for `mdb_cursor_open`)

        Parameters:

        * Transaction object
        * Database instance object
    */
    static NAN_METHOD(ctor);

    /*
        Closes the cursor.
        (Wrapper for `mdb_cursor_close`)

        Parameters:

        * Transaction object
        * Database instance object
    */
    static NAN_METHOD(close);

    // Helper method for getters (not exposed)
    static Nan::NAN_METHOD_RETURN_TYPE getCommon(
        Nan::NAN_METHOD_ARGS_TYPE info, MDB_cursor_op op,
        void (*setKey)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&),
        void (*setData)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&),
        void (*freeData)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&),
        Local<Value> (*convertFunc)(MDB_val &data));

    // Helper method for getters (not exposed)
    static Nan::NAN_METHOD_RETURN_TYPE getCommon(Nan::NAN_METHOD_ARGS_TYPE info, MDB_cursor_op op);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentString);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentBinary);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentNumber);

    /*
        Gets the current key-data pair that the cursor is pointing to.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentBoolean);

    /*
        Gets the current key-data pair that the cursor is pointing to.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(get);

    /*
        Asks the cursor to go to the first key-data pair in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToFirst);

    /*
        Asks the cursor to go to the last key-data pair in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToLast);

    /*
        Asks the cursor to go to the next key-data pair in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToNext);

    /*
        Asks the cursor to go to the previous key-data pair in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToPrev);

    /*
        Asks the cursor to go to the specified key in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToKey);

    /*
        Asks the cursor to go to the first key greater than or equal to the specified parameter in the database.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToRange);

    /*
        For databases with the dupSort option. Asks the cursor to go to the first occurence of the current key.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToFirstDup);

    /*
        For databases with the dupSort option. Asks the cursor to go to the last occurence of the current key.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToLastDup);

    /*
        For databases with the dupSort option. Asks the cursor to go to the next occurence of the current key.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToNextDup);

    /*
        For databases with the dupSort option. Asks the cursor to go to the previous occurence of the current key.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToPrevDup);

    /*
        For databases with the dupSort option. Asks the cursor to go to the specified key/data pair.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToDup);

    /*
        For databases with the dupSort option. Asks the cursor to go to the specified key with the first data that is greater than or equal to the specified.
        (Wrapper for `mdb_cursor_get`)
    */
    static NAN_METHOD(goToDupRange);

    /*
        Deletes the key/data pair to which the cursor refers.
        (Wrapper for `mdb_cursor_del`)
    */
    static NAN_METHOD(del);
};

// External string resource that glues MDB_val and v8::String
class CustomExternalStringResource : public String::ExternalStringResource {
private:
    const uint16_t *d;
    size_t l;

public:
    CustomExternalStringResource(MDB_val *val);
    ~CustomExternalStringResource();

    void Dispose();
    const uint16_t *data() const;
    size_t length() const;

    static void writeTo(Handle<String> str, MDB_val *val, char type = TYPE_STRING);
};

struct NumberData {
    char type;
    double data;
};

struct BooleanData {
    char type;
    bool data;
};

void attachJson();
Local<String> ValueToJson(Handle<Value> value);
Local<Value> ValueFromJson(Handle<String> json);

#endif // NODE_LMDB_H
