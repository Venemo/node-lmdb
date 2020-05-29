
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

#include "node-lmdb.h"

#ifdef WIN32
#include <windows.h>
#pragma comment(lib, "user32.lib")
#else
#include <unistd.h>
#endif

using namespace v8;
using namespace node;

Nan::Persistent<Function> EnvWrap::txnCtor;

Nan::Persistent<Function> EnvWrap::dbiCtor;

typedef struct EnvSyncData {
    uv_work_t request;
    Nan::Callback *callback;
    EnvWrap *ew;
    MDB_env *env;
    int rc;
} EnvSyncData;

EnvWrap::EnvWrap() {
    this->env = nullptr;
}

EnvWrap::~EnvWrap() {
    // Close if not closed already
    if (this->env) {
        mdb_env_close(env);
    }
}

NAN_METHOD(EnvWrap::ctor) {
    int rc;

    EnvWrap* ew = new EnvWrap();
    rc = mdb_env_create(&(ew->env));

    if (rc != 0) {
        mdb_env_close(ew->env);
        if (throwLMDBError(rc)) {
            return;
        }
    }

    ew->Wrap(info.This());
    ew->Ref();

    NanReturnThis();
}

template<class T>
int applyUint32Setting(int (*f)(MDB_env *, T), MDB_env* e, Local<Object> options, T dflt, const char* keyName) {
    int rc;
    const Local<Value> value = Nan::Get(options, Nan::New<String>(keyName).ToLocalChecked()).ToLocalChecked();
    if (value->IsUint32()) {
        rc = f(e, value->Uint32Value(Nan::GetCurrentContext()).FromJust());
    }
    else {
        rc = f(e, dflt);
    }

    return rc;

}

NAN_METHOD(EnvWrap::open) {
    int rc;
    int flags = 0;

    // Get the wrapper
    EnvWrap *ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());

    if (!ew->env) {
        return Nan::ThrowError("The environment is already closed.");
    }

    Local<Object> options = info[0]->ToObject(Nan::GetCurrentContext()).FromMaybe(v8::Local<v8::Object>());
    Local<String> path = Nan::Get(options, Nan::New<String>("path").ToLocalChecked()).ToLocalChecked()->ToString(Nan::GetCurrentContext()).FromMaybe(v8::Local<v8::String>());

    // Parse the maxDbs option
    rc = applyUint32Setting<unsigned>(&mdb_env_set_maxdbs, ew->env, options, 10, "maxDbs");
    if (throwLMDBError(rc)) {
        return;
    }

    // Parse the mapSize option
    Local<Value> mapSizeOption = Nan::Get(options, Nan::New<String>("mapSize").ToLocalChecked()).ToLocalChecked();
    if (mapSizeOption->IsNumber()) {
        double mapSizeDouble = mapSizeOption->NumberValue(Nan::GetCurrentContext()).FromJust();
        size_t mapSizeSizeT = (size_t) mapSizeDouble;
        rc = mdb_env_set_mapsize(ew->env, mapSizeSizeT);
        if (throwLMDBError(rc)) {
            return;
        }
    }

    // Parse the maxDbs option
    // NOTE: mdb.c defines DEFAULT_READERS as 126
    rc = applyUint32Setting<unsigned>(&mdb_env_set_maxreaders, ew->env, options, 126, "maxReaders");
    if (throwLMDBError(rc)) {
        return;
    }

    // NOTE: MDB_FIXEDMAP is not exposed here since it is "highly experimental" + it is irrelevant for this use case
    // NOTE: MDB_NOTLS is not exposed here because it is irrelevant for this use case, as node will run all this on a single thread anyway
    setFlagFromValue(&flags, MDB_NOSUBDIR, "noSubdir", false, options);
    setFlagFromValue(&flags, MDB_RDONLY, "readOnly", false, options);
    setFlagFromValue(&flags, MDB_WRITEMAP, "useWritemap", false, options);
    setFlagFromValue(&flags, MDB_NOMETASYNC, "noMetaSync", false, options);
    setFlagFromValue(&flags, MDB_NOSYNC, "noSync", false, options);
    setFlagFromValue(&flags, MDB_MAPASYNC, "mapAsync", false, options);

    // Set MDB_NOTLS to enable multiple read-only transactions on the same thread (in this case, the nodejs main thread)
    flags |= MDB_NOTLS;

    // TODO: make file attributes configurable
    rc = mdb_env_open(ew->env, *Nan::Utf8String(path), flags, 0664);

    if (rc != 0) {
        mdb_env_close(ew->env);
        ew->env = nullptr;
        if (throwLMDBError(rc)) {
            return;
        }
    }

    return;
}

NAN_METHOD(EnvWrap::close) {
    EnvWrap *ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    ew->Unref();

    if (!ew->env) {
        return Nan::ThrowError("The environment is already closed.");
    }

    mdb_env_close(ew->env);
    ew->env = nullptr;

    return;
}

NAN_METHOD(EnvWrap::beginTxn) {
    const int argc = 2;

    Local<Value> argv[argc] = { info.This(), info[0] };
    auto instance = Nan::NewInstance(Nan::New(txnCtor), argc, argv);

    if (instance.IsEmpty()) {
        return;
    }

    info.GetReturnValue().Set(instance.ToLocalChecked());
}

NAN_METHOD(EnvWrap::openDbi) {
    const unsigned argc = 2;
    Local<Value> argv[argc] = { info.This(), info[0] };
    auto instance = Nan::NewInstance(Nan::New(dbiCtor), argc, argv);

    if (instance.IsEmpty()) {
        return;
    }

    info.GetReturnValue().Set(instance.ToLocalChecked());
}

NAN_METHOD(EnvWrap::sync) {
    EnvWrap *ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());

    if (!ew->env) {
        return Nan::ThrowError("The environment is already closed.");
    }

    Local<Function> callback = info[0].As<Function>();

    EnvSyncData *d = new EnvSyncData;
    d->request.data = d;
    d->ew = ew;
    d->env = ew->env;
    d->callback = new Nan::Callback(callback);

    uv_queue_work(uv_default_loop(), &(d->request), [](uv_work_t *request) -> void {
        // Performing the sync (this will be called on a separate thread)
        EnvSyncData *d = static_cast<EnvSyncData*>(request->data);
        d->rc = mdb_env_sync(d->env, 1);
    }, [](uv_work_t *request, int) -> void {
        Nan::HandleScope scope;

        // Executed after the sync is finished
        EnvSyncData *d = static_cast<EnvSyncData*>(request->data);
        const unsigned argc = 1;
        Local<Value> argv[argc];

        if (d->rc == 0) {
            argv[0] = Nan::Null();
        }
        else {
            argv[0] = Nan::Error(mdb_strerror(d->rc));
            Nan::Set(argv[0].As<Object>(), Nan::New("code").ToLocalChecked(), Nan::New(d->rc));
            //argv[0].As<Object>()->Set(Nan::New("code").ToLocalChecked(), Nan::New(d->rc));
        }

        d->callback->Call(argc, argv);
        delete d->callback;
        delete d;
    });

    info.GetReturnValue().SetUndefined();
}

inline int getOSPageSize() {
    int result = 0;

#ifdef WIN32
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    result = siSysInfo.dwPageSize;
#else
    result = getpagesize();
#endif

    return result;
}

NAN_METHOD(EnvWrap::getOSPageSize) {
    return info.GetReturnValue().Set(Nan::New(::getOSPageSize()));
}

NAN_METHOD(EnvWrap::getStat) {
    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    MDB_stat stat;
    int err = mdb_env_stat(env, &stat);
    if (throwLMDBError(err)) {
        return;
    }
    auto result = Nan::New<Object>();
    Nan::Set(result, Nan::New<String>("pSize").ToLocalChecked(), Nan::New((unsigned)stat.ms_psize));
    Nan::Set(result, Nan::New<String>("depth").ToLocalChecked(), Nan::New((unsigned)stat.ms_depth));
    Nan::Set(result, Nan::New<String>("branchPages").ToLocalChecked(), Nan::New((unsigned)stat.ms_branch_pages));
    Nan::Set(result, Nan::New<String>("leafPages").ToLocalChecked(), Nan::New((unsigned)stat.ms_leaf_pages));
    Nan::Set(result, Nan::New<String>("overflowPages").ToLocalChecked(), Nan::New((unsigned)stat.ms_overflow_pages));
    Nan::Set(result, Nan::New<String>("entries").ToLocalChecked(), Nan::New((unsigned)stat.ms_entries));

    return info.GetReturnValue().Set(result);
}

NAN_METHOD(EnvWrap::getInfo) {
    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    MDB_envinfo inf;
    int err = mdb_env_info(env, &inf);
    if (throwLMDBError(err)) {
        return;
    }
    auto result = Nan::New<Object>();
    Nan::Set(result, Nan::New<String>("mapSize").ToLocalChecked(), Nan::New((unsigned)inf.me_mapsize));
    Nan::Set(result, Nan::New<String>("lastPgNo").ToLocalChecked(), Nan::New((unsigned)inf.me_last_pgno));
    Nan::Set(result, Nan::New<String>("lastTxnId").ToLocalChecked(), Nan::New((unsigned)inf.me_last_txnid));
    Nan::Set(result, Nan::New<String>("maxReaders").ToLocalChecked(), Nan::New((unsigned)inf.me_maxreaders));
    Nan::Set(result, Nan::New<String>("numReaders").ToLocalChecked(), Nan::New((unsigned)inf.me_numreaders));

    return info.GetReturnValue().Set(result);
}

NAN_METHOD(EnvWrap::getUsedSize) {
    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    MDB_stat stat;
    MDB_envinfo inf;
    int err = mdb_env_stat(env, &stat);
    if (throwLMDBError(err)) {
        return;
    }
    err = mdb_env_info(env, &inf);
    if (throwLMDBError(err)) {
        return;
    }

    return info.GetReturnValue().Set(Nan::New((unsigned)inf.me_last_pgno * stat.ms_psize));
}

NAN_METHOD(EnvWrap::setMapSize) {
    unsigned size = info[0]->Uint32Value(Nan::GetCurrentContext()).FromJust();

    if (size % ::getOSPageSize() != 0) {
        return Nan::ThrowError("Size must be a multiplier of OS\' page size.");
    }

    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    int err = mdb_env_set_mapsize(env, size);
    if (throwLMDBError(err)) {
        return;
    }

    return info.GetReturnValue().SetUndefined();
}

NAN_METHOD(EnvWrap::getMaxReaders) {
    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    unsigned num;
    int err = mdb_env_get_maxreaders(env, &num);
    if (throwLMDBError(err)) {
        return;
    }

    return info.GetReturnValue().Set(Nan::New(num));
}

NAN_METHOD(EnvWrap::setMaxReaders) {
    unsigned num = info[0]->Uint32Value(Nan::GetCurrentContext()).FromJust();

    auto ew = Nan::ObjectWrap::Unwrap<EnvWrap>(info.This());
    auto env = ew->env;
    int err = mdb_env_set_maxreaders(env, num);
    if (throwLMDBError(err)) {
        return;
    }

    return info.GetReturnValue().SetUndefined();
}

void EnvWrap::setupExports(Local<Object> exports) {
    // EnvWrap: Prepare constructor template
    Local<FunctionTemplate> envTpl = Nan::New<FunctionTemplate>(EnvWrap::ctor);
    envTpl->SetClassName(Nan::New<String>("Env").ToLocalChecked());
    envTpl->InstanceTemplate()->SetInternalFieldCount(1);
    // EnvWrap: Add functions to the prototype
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("open").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::open));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("close").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::close));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("beginTxn").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::beginTxn));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("openDbi").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::openDbi));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("sync").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::sync));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("getStat").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::getStat));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("getInfo").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::getInfo));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("getUsedSize").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::getUsedSize));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("setMapSize").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::setMapSize));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("getMaxReaders").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::getMaxReaders));
    envTpl->PrototypeTemplate()->Set(Nan::New<String>("setMaxReaders").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::setMaxReaders));
    // TODO: wrap mdb_env_copy too

    auto envCtor = envTpl->GetFunction(Nan::GetCurrentContext()).ToLocalChecked();
    Nan::Set(envCtor, Nan::New<String>("getOSPageSize").ToLocalChecked(), Nan::New<FunctionTemplate>(EnvWrap::getOSPageSize)->GetFunction(Nan::GetCurrentContext()).ToLocalChecked());

    // TxnWrap: Prepare constructor template
    Local<FunctionTemplate> txnTpl = Nan::New<FunctionTemplate>(TxnWrap::ctor);
    txnTpl->SetClassName(Nan::New<String>("Txn").ToLocalChecked());
    txnTpl->InstanceTemplate()->SetInternalFieldCount(1);
    // TxnWrap: Add functions to the prototype
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("commit").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::commit));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("abort").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::abort));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getString").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getString));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getStringUnsafe").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getStringUnsafe));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getBinary").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getBinary));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getBinaryUnsafe").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getBinaryUnsafe));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getNumber").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getNumber));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("getBoolean").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::getBoolean));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("putString").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::putString));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("putBinary").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::putBinary));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("putNumber").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::putNumber));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("putBoolean").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::putBoolean));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("del").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::del));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("reset").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::reset));
    txnTpl->PrototypeTemplate()->Set(Nan::New<String>("renew").ToLocalChecked(), Nan::New<FunctionTemplate>(TxnWrap::renew));
    // TODO: wrap mdb_cmp too
    // TODO: wrap mdb_dcmp too
    // TxnWrap: Get constructor
    EnvWrap::txnCtor.Reset( txnTpl->GetFunction(Nan::GetCurrentContext()).ToLocalChecked());

    // DbiWrap: Prepare constructor template
    Local<FunctionTemplate> dbiTpl = Nan::New<FunctionTemplate>(DbiWrap::ctor);
    dbiTpl->SetClassName(Nan::New<String>("Dbi").ToLocalChecked());
    dbiTpl->InstanceTemplate()->SetInternalFieldCount(1);
    // DbiWrap: Add functions to the prototype
    dbiTpl->PrototypeTemplate()->Set(Nan::New<String>("close").ToLocalChecked(), Nan::New<FunctionTemplate>(DbiWrap::close));
    dbiTpl->PrototypeTemplate()->Set(Nan::New<String>("drop").ToLocalChecked(), Nan::New<FunctionTemplate>(DbiWrap::drop));
    dbiTpl->PrototypeTemplate()->Set(Nan::New<String>("stat").ToLocalChecked(), Nan::New<FunctionTemplate>(DbiWrap::stat));
    // TODO: wrap mdb_stat too
    // DbiWrap: Get constructor
    EnvWrap::dbiCtor.Reset( dbiTpl->GetFunction(Nan::GetCurrentContext()).ToLocalChecked());

    // Set exports
    Nan::Set(exports, Nan::New<String>("Env").ToLocalChecked(), envCtor);
}
