
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
#include <string.h>
#include <stdio.h>

void setupExportMisc(Handle<Object> exports) {
    Local<Object> versionObj = Nan::New<Object>();

    int major, minor, patch;
    char *str = mdb_version(&major, &minor, &patch);
    versionObj->Set(Nan::New<String>("versionString").ToLocalChecked(), Nan::New<String>(str).ToLocalChecked());
    versionObj->Set(Nan::New<String>("major").ToLocalChecked(), Nan::New<Integer>(major));
    versionObj->Set(Nan::New<String>("minor").ToLocalChecked(), Nan::New<Integer>(minor));
    versionObj->Set(Nan::New<String>("patch").ToLocalChecked(), Nan::New<Integer>(patch));

    exports->Set(Nan::New<String>("version").ToLocalChecked(), versionObj);
}

void setFlagFromValue(int *flags, int flag, const char *name, bool defaultValue, Local<Object> options) {
    Local<Value> opt = options->Get(Nan::New<String>(name).ToLocalChecked());
    if (opt->IsBoolean() ? opt->BooleanValue() : defaultValue) {
        *flags |= flag;
    }
}

argtokey_callback_t argToKey(const Local<Value> &val, MDB_val &key, bool keyIsUint32) {
    // Check key type
    if (keyIsUint32 && !val->IsUint32()) {
        Nan::ThrowError("Invalid key. keyIsUint32 specified on the database, but the given key was not an unsigned 32-bit integer");
        return nullptr;
    }
    if (!keyIsUint32 && !val->IsString()) {
        Nan::ThrowError("Invalid key. String key expected, because keyIsUint32 isn't specified on the database.");
        return nullptr;
    }

    // Handle uint32_t key
    if (keyIsUint32) {
        uint32_t *v = new uint32_t;
        *v = val->Uint32Value();

        key.mv_size = sizeof(uint32_t);
        key.mv_data = v;

        return ([](MDB_val &key) -> void {
            delete (uint32_t*)key.mv_data;
        });
    }

    // Handle string key
    CustomExternalStringResource::writeTo(val->ToString(), &key);
    return ([](MDB_val &key) -> void {
        delete[] (uint16_t*)key.mv_data;
    });

    return nullptr;
}

Local<Value> keyToHandle(MDB_val &key, bool keyIsUint32) {
    if (keyIsUint32) {
        return Nan::New<Integer>(*((uint32_t*)key.mv_data));
    }
    else {
        return valToString(key);
    }
}

Local<Value> valToString(MDB_val &data) {
    auto resource = new CustomExternalStringResource(&data);
    auto str = Nan::New<v8::String>(resource);

    return str.ToLocalChecked();
}

Local<Value> valToBinary(MDB_val &data) {
    // FIXME: It'd be better not to copy buffers, but I'm not sure
    // about the ownership semantics of MDB_val, so let' be safe.
    char* buffer = (char*)data.mv_data;
    buffer++;
    return Nan::CopyBuffer(
        buffer,
        data.mv_size - 1
    ).ToLocalChecked();
}

Local<Value> valToNumber(MDB_val &data) {
    return Nan::New<Number>(((NumberData*)data.mv_data)->data);
}

Local<Value> valToBoolean(MDB_val &data) {
    return Nan::New<Boolean>(((BooleanData*)data.mv_data)->data);
}

Local<Value> valToVal(MDB_val &data) {
    char type = *(char*)data.mv_data;
    if (type == TYPE_NUMBER) {
        return valToNumber(data);
    }
    if (type == TYPE_BOOLEAN) {
        return valToBoolean(data);
    }
    if (type == TYPE_BINARY) {
        return valToBinary(data);
    }
    return valToString(data);
}

void consoleLog(const char *msg) {
    Local<String> str = Nan::New("console.log('").ToLocalChecked();
    str = String::Concat(str, Nan::New<String>(msg).ToLocalChecked());
    str = String::Concat(str, Nan::New("');").ToLocalChecked());

    Local<Script> script = Nan::CompileScript(str).ToLocalChecked();
    Nan::RunScript(script);
}

void consoleLog(Local<Value> val) {
    Local<String> str = Nan::New<String>("console.log('").ToLocalChecked();
    str = String::Concat(str, val->ToString());
    str = String::Concat(str, Nan::New<String>("');").ToLocalChecked());

    Local<Script> script = Nan::CompileScript(str).ToLocalChecked();
    Nan::RunScript(script);
}

void consoleLogN(int n) {
    char c[20];
    memset(c, 0, 20 * sizeof(char));
    sprintf(c, "%d", n);
    consoleLog(c);
}

void CustomExternalStringResource::writeTo(Handle<String> str, MDB_val *val) {
    // first item is data type, string starts from position 1
    unsigned int l = str->Length() + 2;
    uint16_t *d = new uint16_t[l];
    uint16_t *d2 = d;
    d2++;
    str->Write(d2);
    d[l - 1] = 0;
    d[0] = 0;
    *(char*)d = TYPE_STRING;

    val->mv_data = d;
    val->mv_size = l * sizeof(uint16_t);
}

CustomExternalStringResource::CustomExternalStringResource(MDB_val *val) {
    uint16_t *d2 = (uint16_t*)val->mv_data;
    d2++;
    // The UTF-16 data
    this->d = d2;
    // Number of UTF-16 characters in the string
    this->l = (val->mv_size / sizeof(uint16_t) - 2);
}

CustomExternalStringResource::~CustomExternalStringResource() { }

void CustomExternalStringResource::Dispose() {
    // No need to do anything, the data is owned by LMDB, not us

    // But actually need to delete the string resource itself:
    // the docs say that "The default implementation will use the delete operator."
    // while initially I thought this means using delete on the string,
    // apparently they meant just calling the destructor of this class.
    delete this;
}

const uint16_t *CustomExternalStringResource::data() const {
    return this->d;
}

size_t CustomExternalStringResource::length() const {
    return this->l;
}
