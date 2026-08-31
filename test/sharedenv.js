'use strict';

// Regression test for the reference count of environments shared between
// multiple Env objects opened on the same path. Run as a child process
// because a failure is a segmentation fault, not an exception.
// See test/index.test.js, describe('Shared environments').

var path = require('path');
var mkdirp = require('mkdirp');
var rimraf = require('rimraf');
var lmdb = require('..');

var testDirPath = path.resolve(__dirname, './testdata-sharedenv');

rimraf.sync(testDirPath);
mkdirp.sync(testDirPath);

// Two Env objects on the same path share a single MDB_env, so the cached
// entry must be counted twice. If the count is not incremented, closing the
// second Env frees the environment that the first one is still using.
var env1 = new lmdb.Env();
env1.open({ path: testDirPath, maxDbs: 4 });

var env2 = new lmdb.Env();
env2.open({ path: testDirPath, maxDbs: 4 });

env2.close();

// This dereferences the environment of the first Env object, which used to
// be a dangling pointer at this point.
var dbi = env1.openDbi({ name: 'sharedenv', create: true });
var txn = env1.beginTxn();
txn.putString(dbi, 'key', 'value');
txn.commit();

var txn2 = env1.beginTxn({ readOnly: true });
var value = txn2.getString(dbi, 'key');
txn2.abort();

dbi.close();
env1.close();

rimraf.sync(testDirPath);

if (value !== 'value') {
    console.error('Expected "value" but read ' + JSON.stringify(value));
    process.exit(1);
}

console.log('Environment shared between two Env objects survived closing one of them');
