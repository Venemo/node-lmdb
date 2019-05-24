
// This file is part of node-lmdb, the Node.js binding for lmdb
// Copyright (c) 2013-2017 Timur Kristóf
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

#include <vector>
#include <algorithm>
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <uv.h>
#include "lmdb.h"

using namespace v8;
using namespace node;

enum class NodeLmdbKeyType {

    // Invalid key (used internally by node-lmdb)
    InvalidKey = -1,
    
    // Default key (used internally by node-lmdb)
    DefaultKey = 0,

    // UCS-2/UTF-16 with zero terminator - Appears to V8 as string
    StringKey = 1,
    
    // LMDB fixed size integer key with 32 bit keys - Appearts to V8 as an Uint32
    Uint32Key = 2,
    
    // LMDB default key format - Appears to V8 as node::Buffer
    BinaryKey = 3,

};

// Exports misc stuff to the module
void setupExportMisc(Handle<Object> exports);

// Helper callback
typedef void (*argtokey_callback_t)(MDB_val &key);

void consoleLog(Local<Value> val);
void consoleLog(const char *msg);
void consoleLogN(int n);
void setFlagFromValue(int *flags, int flag, const char *name, bool defaultValue, Local<Object> options);
argtokey_callback_t argToKey(const Local<Value> &val, MDB_val &key, NodeLmdbKeyType keyType, bool &isValid);
NodeLmdbKeyType inferAndValidateKeyType(const Local<Value> &key, const Local<Value> &options, NodeLmdbKeyType dbiKeyType, bool &isValid);
NodeLmdbKeyType inferKeyType(const Local<Value> &val);
NodeLmdbKeyType keyTypeFromOptions(const Local<Value> &val, NodeLmdbKeyType defaultKeyType = NodeLmdbKeyType::StringKey);
Local<Value> keyToHandle(MDB_val &key, NodeLmdbKeyType keyType);

Local<Value> valToString(MDB_val &data);
Local<Value> valToStringUnsafe(MDB_val &data);
Local<Value> valToBinary(MDB_val &data);
Local<Value> valToBinaryUnsafe(MDB_val &data);
Local<Value> valToNumber(MDB_val &data);
Local<Value> valToBoolean(MDB_val &data);

void throwLmdbError(int rc);

class TxnWrap;
class DbiWrap;
class EnvWrap;
class CursorWrap;

/*
    `Env`
    Represents a database environment.
    (Wrapper for `MDB_env`)
*/
class EnvWrap : public Nan::ObjectWrap {
private:
    // The wrapped object
    MDB_env *env;
    // Current write transaction
    TxnWrap *currentWriteTxn;
    // List of open read transactions
    std::vector<TxnWrap*> readTxns;
    // Constructor for TxnWrap
    static Nan::Persistent<Function> txnCtor;
    // Constructor for DbiWrap
    static Nan::Persistent<Function> dbiCtor;
    
    // Cleans up stray transactions
    void cleanupStrayTxns();

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
        Gets statistics about the database environment.
    */
    static NAN_METHOD(stat);
    
    /*
        Gets information about the database environment.
    */
    static NAN_METHOD(info);

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
        Resizes the maximal size of the memory map. It may be called if no transactions are active in this process.
        (Wrapper for `mdb_env_set_mapsize`)

        Parameters:

        * maximal size of the memory map (the full environment) in bytes (default is 10485760 bytes)
    */
    static NAN_METHOD(resize);

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

    /*
        Performs a set of operations asynchronously, automatically wrapping it in its own transaction

        Parameters:

        * Callback to be executed after the sync is complete.
    */
    static NAN_METHOD(batchWrite);
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

    // Environment wrapper of the current transaction
    EnvWrap *ew;
    
    // Flags used with mdb_txn_begin
    unsigned int flags;
    
    // Remove the current TxnWrap from its EnvWrap
    void removeFromEnvWrap();

    friend class CursorWrap;
    friend class DbiWrap;
    friend class EnvWrap;

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
        This method is not zero-copy and the return value will usable as long as there is a reference to it.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getString);

    /*
        Gets string data (JavaScript string type) associated with the given key from a database. You need to open a database in the environment to use this.
        This method is zero-copy and the return value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getStringUnsafe);

    /*
        Gets binary data (Node.js Buffer) associated with the given key from a database. You need to open a database in the environment to use this.
        This method is not zero-copy and the return value will usable as long as there is a reference to it.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getBinary);

    /*
        Gets binary data (Node.js Buffer) associated with the given key from a database. You need to open a database in the environment to use this.
        This method is zero-copy and the return value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_get`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is retrieved
    */
    static NAN_METHOD(getBinaryUnsafe);

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
        Puts string data (JavaScript string type) into a database.
        (Wrapper for `mdb_put`)

        Parameters:

        * database instance created with calling `openDbi()` on an `Env` instance
        * key for which the value is stored
        * data to store for the given key
    */
    static NAN_METHOD(putString);

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
    // Tells how keys should be treated
    NodeLmdbKeyType keyType;
    // Stores flags set when opened
    int flags;
    // The wrapped object
    MDB_dbi dbi;
    // Reference to the MDB_env of the wrapped MDB_dbi
    MDB_env *env;
    // The EnvWrap object of the current Dbi
    EnvWrap *ew;
    // Whether the Dbi was opened successfully
    bool isOpen;

    friend class TxnWrap;
    friend class CursorWrap;
    friend class EnvWrap;

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
    // Stores how key is represented
    NodeLmdbKeyType keyType;
    // Key/data pair where the cursor is at
    MDB_val key, data;
    // Free function for the current key
    argtokey_callback_t freeKey;
    
    DbiWrap *dw;
    TxnWrap *tw;
    
    template<size_t keyIndex, size_t optionsIndex>
    friend argtokey_callback_t cursorArgToKey(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val &key, bool &keyIsValid);

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
        argtokey_callback_t (*setKey)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&, bool&),
        void (*setData)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&),
        void (*freeData)(CursorWrap* cw, Nan::NAN_METHOD_ARGS_TYPE info, MDB_val&),
        Local<Value> (*convertFunc)(MDB_val &data));

    // Helper method for getters (not exposed)
    static Nan::NAN_METHOD_RETURN_TYPE getCommon(Nan::NAN_METHOD_ARGS_TYPE info, MDB_cursor_op op);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        This method is not zero-copy and the return value will usable as long as there is a reference to it.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentString);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        This method is zero-copy and the value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentStringUnsafe);

    /*
        Gets the current key-data pair that the cursor is pointing to. Returns the current key.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentBinary);


    /*
        Gets the current key-data pair with zero-copy that the cursor is pointing to. Returns the current key.
        This method is zero-copy and the value can only be used until the next put operation or until the transaction is committed or aborted.
        (Wrapper for `mdb_cursor_get`)

        Parameters:

        * Callback that accepts the key and value
    */
    static NAN_METHOD(getCurrentBinaryUnsafe);

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

    static void writeTo(Handle<String> str, MDB_val *val);
};

#endif // NODE_LMDB_H
