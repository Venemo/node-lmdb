'use strict';

var path = require('path');
var mkdirp = require('mkdirp');
var rimraf = require('rimraf');
var chai = require('chai');
var should = chai.should();
var spawn = require('child_process').spawn;

var lmdb = require('..');

describe('Node.js LMDB Bindings', function() {
  var testDirPath = path.resolve(__dirname, './testdata');
  before(function(done) {
    // cleanup previous test directory
    rimraf(testDirPath, function(err) {
      if (err) {
        return done(err);
      }
      // setup clean directory
      mkdirp(testDirPath, function(err) {
        if (err) {
          return done(err);
        }
        done();
      });
    });
  });
  it('will construct, open and close an environment', function() {
    var env = new lmdb.Env();
    env.open({
      path: testDirPath,
      maxDbs: 10
    });
    env.close.should.be.a('function');
    env.beginTxn.should.be.a('function');
    env.openDbi.should.be.a('function');
    env.sync.should.be.a('function');
    env.close();
  });
  describe('Basics', function() {
    var env;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10
      });
    });
    after(function() {
      env.close();
    });
    it('will open a database, begin a transaction and get/put/delete data', function() {
      var dbi = env.openDbi({
        name: 'mydb1',
        create: true
      });
      var txn = env.beginTxn();
      var data = txn.getString(dbi, 'hello');
      should.equal(data, null);
      txn.putString(dbi, 'hello', 'Hello world!');
      var data2 = txn.getString(dbi, 'hello');
      data2.should.equal('Hello world!');
      txn.del(dbi, 'hello');
      var data3 = txn.getString(dbi, 'hello');
      should.equal(data3, null);
      txn.commit();
      dbi.close();
    });
    it('will throw Javascript error if named database cannot be found', function () {
      try {
        env.openDbi({
          name: 'does-not-exist',
          create: false
        });
      } catch (err) {
        err.should.be.an.instanceof(Error);
      }
    });
    it('will get statistics for a database', function() {
      var dbi = env.openDbi({
        name: 'mydb2',
        create: true
      });
      var txn = env.beginTxn();
      var stat = dbi.stat(txn);
      stat.pageSize.should.be.a('number');
      stat.treeDepth.should.be.a('number');
      stat.treeBranchPageCount.should.be.a('number');
      stat.treeLeafPageCount.should.be.a('number');
      stat.entryCount.should.be.a('number');
      txn.abort();
      dbi.close();
    });
  });
  describe('Data types', function() {
    var env;
    var dbi;
    var txn;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10
      });
      dbi = env.openDbi({
        name: 'mydb3',
        create: true
      });
      txn = env.beginTxn();
    });
    after(function() {
      txn.commit();
      dbi.close();
      env.close();
    });
    it('string', function() {
      txn.putString(dbi, 'key1', 'Hello world!');
      var data = txn.getString(dbi, 'key1');
      data.should.equal('Hello world!');
      txn.del(dbi, 'key1');
      var data2 = txn.getString(dbi, 'key1');
      should.equal(data2, null);
    });
    it('string (zero copy)', function() {
      txn.putString(dbi, 'key1', 'Hello world!');
      var data = txn.getStringUnsafe(dbi, 'key1');
      data.should.equal('Hello world!');
      txn.del(dbi, 'key1');
      var data2 = txn.getStringUnsafe(dbi, 'key1');
      should.equal(data2, null);
    });
    it('binary', function() {
      var buffer = new Buffer('48656c6c6f2c20776f726c6421', 'hex');
      txn.putBinary(dbi, 'key2', buffer);
      var data = txn.getBinary(dbi, 'key2');
      data.should.deep.equal(buffer);
      txn.del(dbi, 'key2');
      var data2 = txn.getBinary(dbi, 'key2');
      should.equal(data2, null);
    });
    it('binary (zero copy)', function() {
      var buffer = new Buffer('48656c6c6f2c20776f726c6421', 'hex');
      txn.putBinary(dbi, 'key2', buffer);
      var data = txn.getBinaryUnsafe(dbi, 'key2');
      data.should.deep.equal(buffer);
      txn.del(dbi, 'key2');
      var data2 = txn.getBinaryUnsafe(dbi, 'key2');
      should.equal(data2, null);
    });
    it('binary key', function() {
      var buffer = new Buffer('48656c6c6f2c20776f726c6421', 'hex');
      var key = new Buffer('key2');
      txn.putBinary(dbi, key, buffer);
      var data = txn.getBinary(dbi, key);
      data.should.deep.equal(buffer);
      txn.del(dbi, key);
      var data2 = txn.getBinary(dbi, key);
      should.equal(data2, null);
    });
    it('number', function() {
      txn.putNumber(dbi, 'key3', 9007199254740991);
      var data = txn.getNumber(dbi, 'key3');
      data.should.equal(Math.pow(2, 53) - 1);
      txn.del(dbi, 'key3');
      var data2 = txn.getNumber(dbi, 'key3');
      should.equal(data2, null);
    });
    it('boolean', function() {
      txn.putBoolean(dbi, 'key4', true);
      var data = txn.getBoolean(dbi, 'key4');
      data.should.equal(true);
      txn.putBoolean(dbi, 'key5', false);
      var data2 = txn.getBoolean(dbi, 'key5');
      data2.should.equal(false);
      txn.del(dbi, 'key4');
      txn.del(dbi, 'key5');
      var data3 = txn.getBoolean(dbi, 'key4');
      var data4 = txn.getBoolean(dbi, 'key5');
      should.equal(data3, null);
      should.equal(data4, null);
    });
  });
  describe('Multiple transactions', function() {
    var env;
    var dbi;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10
      });
      dbi = env.openDbi({
        name: 'mydb4',
        create: true,
        keyIsUint32: true
      });
      var txn = env.beginTxn();
      txn.putString(dbi, 1, 'Hello1');
      txn.putString(dbi, 2, 'Hello2');
      txn.commit();
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('readonly transaction should not see uncommited changes', function() {
      var readTxn = env.beginTxn({readOnly: true});
      var data = readTxn.getString(dbi, 1);
      should.equal(data, 'Hello1');

      var writeTxn = env.beginTxn();
      writeTxn.putString(dbi, 1, 'Ha ha ha');

      var data2 = writeTxn.getString(dbi, 1);
      data2.should.equal('Ha ha ha');

      var data3 = readTxn.getString(dbi, 1);
      should.equal(data3, 'Hello1');

      writeTxn.commit();
      var data4 = readTxn.getString(dbi, 1);
      should.equal(data4, 'Hello1');

      readTxn.reset();
      readTxn.renew();
      var data5 = readTxn.getString(dbi, 1);
      should.equal(data5, 'Ha ha ha');
      readTxn.abort();
    });
    it('readonly transaction will throw if tries to write', function() {
      var readTxn = env.beginTxn({readOnly: true});
      (function() {
        readTxn.putString(dbi, 2, 'hööhh');
      }).should.throw('Permission denied');
      readTxn.abort();
    });
  });
  describe('Cursors', function() {
    this.timeout(10000);
    var env;
    var dbi;
    var total = 100000;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10,
        mapSize: 64 * 1024 * 1024
      });
      dbi = env.openDbi({
        name: 'mydb5',
        create: true,
        dupSort: true,
        keyIsUint32: true
      });
      var txn = env.beginTxn();
      var c = 0;
      while(c < total) {
        var buffer = new Buffer(new Array(8));
        buffer.writeDoubleBE(c);
        txn.putBinary(dbi, c, buffer);
        c++;
      }
      txn.commit();
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('will move cursor over key/values', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToKey(40);
      cursor.getCurrentBinary(function(key, value) {
        key.should.equal(40);
        value.readDoubleBE().should.equal(40);
      });

      var values = [];
      cursor.goToKey(0);
      function iterator() {
        cursor.getCurrentBinary(function(key, value) {
          value.readDoubleBE().should.equal(values.length);
          values.push(value);
        });
        cursor.goToNext();
        if (values.length < total) {
          // prevent maximum call stack errors
          if (values.length % 1000 === 0) {
            setImmediate(iterator);
          } else {
            iterator();
          }
        } else {
          cursor.close();
          txn.abort();
          done();
        }
      }
      iterator();
    });
    it('will move cursor over key/values (zero copy)', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToKey(40);
      cursor.getCurrentBinaryUnsafe(function(key, value) {
        key.should.equal(40);
        value.readDoubleBE().should.equal(40);
      });
      var values = [];
      cursor.goToKey(0);
      function iterator() {
        cursor.getCurrentBinaryUnsafe(function(key, value) {
          value.readDoubleBE().should.equal(values.length);
          values.push(value);
        });
        cursor.goToNext();
        if (values.length < total) {
          // prevent maximum call stack errors
          if (values.length % 1000 === 0) {
            setImmediate(iterator);
          } else {
            iterator();
          }
        } else {
          cursor.close();
          txn.abort();
          done();
        }
      }
      iterator();
    });
    it('will first/last key', function() {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToFirst();
      cursor.getCurrentBinary(function(key, value) {
        key.should.equal(0);
        value.readDoubleBE().should.equal(0);
      });
      cursor.goToLast();
      cursor.getCurrentBinary(function(key, value) {
        key.should.equal(total - 1);
        value.readDoubleBE().should.equal(total - 1);
      });
      cursor.close();
      txn.abort();
    });
  });
  describe('Cursors (with strings)', function() {
    this.timeout(10000);
    var env;
    var dbi;
    var total = 100000;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10,
        mapSize: 64 * 1024 * 1024
      });
      dbi = env.openDbi({
        name: 'cursorstrings',
        create: true,
        dupSort: true,
        keyIsUint32: true
      });
      var txn = env.beginTxn();
      var c = 0;
      while(c < total) {
        txn.putString(dbi, c, c.toString());
        c++;
      }
      txn.commit();
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('will move cursor over key/values (zero copy)', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToKey(40);
      cursor.getCurrentStringUnsafe(function(key, value) {
        key.should.equal(40);
        value.should.equal('40');
      });

      var values = [];
      cursor.goToKey(0);
      function iterator() {
        cursor.getCurrentStringUnsafe(function(key, value) {
          value.should.equal(values.length.toString());
          values.push(value);
        });
        cursor.goToNext();
        if (values.length < total) {
          // prevent maximum call stack errors
          if (values.length % 1000 === 0) {
            setImmediate(iterator);
          } else {
            iterator();
          }
        } else {
          cursor.close();
          txn.abort();
          done();
        }
      }
      iterator();
    });
    it('will move cursor over key/values', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToKey(40);
      cursor.getCurrentString(function(key, value) {
        key.should.equal(40);
        value.should.equal('40');
      });
      var values = [];
      cursor.goToKey(0);
      function iterator() {
        cursor.getCurrentString(function(key, value) {
          value.should.equal(values.length.toString());
          values.push(value);
        });
        cursor.goToNext();
        if (values.length < total) {
          // prevent maximum call stack errors
          if (values.length % 1000 === 0) {
            setImmediate(iterator);
          } else {
            iterator();
          }
        } else {
          cursor.close();
          txn.abort();
          done();
        }
      }
      iterator();
    });
    it('will first/last key', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToFirst();
      cursor.getCurrentString(function(key, value) {
        key.should.equal(0);
        value.should.equal('0');
      });
      cursor.goToLast();
      cursor.getCurrentString(function(key, value) {
        key.should.equal(total - 1);
        value.should.equal((total - 1).toString());
      });
      cursor.close();
      txn.abort();
      done();
    });
  });
  describe('Cluster', function() {
    it('will run a cluster of processes with read-only transactions', function(done) {
      var child = spawn('node', [path.resolve(__dirname, './cluster')]);
      child.stdout.on('data', function(data) {
        console.log(data.toString());
      });
      child.stderr.on('data', function(data) {
        console.error(data.toString());
      });
      child.on('close', function(code) {
        code.should.equal(0);
        done();
      });
    });
  });
  describe('Dupsort', function() {
    this.timeout(10000);
    var env;
    var dbi;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10,
        mapSize:  64 * 1024 * 1024
      });
      dbi = env.openDbi({
        name: 'mydb6',
        create: true,
        dupSort: true,
        dupFixed: false
      });
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('will insert values with different lengths', function(done) {
      var txn = env.beginTxn();
      var value1 = new Buffer(new Array(8));
      var value2 = new Buffer(new Array(4));
      txn.putBinary(dbi, new Buffer('id'), value1);
      txn.putBinary(dbi, new Buffer('id'), value2);
      txn.commit();

      var txn2 = env.beginTxn({readonly: true});
      var cursor = new lmdb.Cursor(txn2, dbi);
      var found = cursor.goToKey(new Buffer('id'));
      should.exist(found);
      cursor.getCurrentBinary(function(key, value) {
        key.toString().should.equal('id');
        value.length.should.equal(4);

        cursor.goToNext();
        cursor.getCurrentBinary(function(key, value) {
          key.toString().should.equal('id');
          value.length.should.equal(8);
          cursor.close();
          txn2.abort();
          done();
        });
      });
    });
  });
  describe('Dupfixed', function() {
    this.timeout(10000);
    var env;
    var dbi;
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 10,
        mapSize:  64 * 1024 * 1024
      });
      dbi = env.openDbi({
        name: 'mydb7',
        create: true,
        dupSort: true,
        dupFixed: true
      });
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('will insert values with the same length (inserted with different lengths)', function(done) {
      var txn = env.beginTxn();
      var value1 = new Buffer(new Array(4));
      value1.writeUInt32BE(100);
      var value2 = new Buffer(new Array(8));
      value2.writeUInt32BE(200);
      txn.putBinary(dbi, new Buffer('id'), value1);
      txn.putBinary(dbi, new Buffer('id'), value2);
      txn.commit();

      var txn2 = env.beginTxn({readonly: true});
      var cursor = new lmdb.Cursor(txn2, dbi);
      var found = cursor.goToKey(new Buffer('id'));
      should.exist(found);
      cursor.getCurrentBinary(function(key, value) {
        key.toString().should.equal('id');
        value.length.should.equal(8);

        cursor.goToNext();
        cursor.getCurrentBinary(function(key, value) {
          key.toString().should.equal('id');
          value.length.should.equal(8);
          cursor.close();
          txn2.abort();
          done();
        });
      });
    });
  });
  describe('Memory Freeing / Garbage Collection', function() {
    it('should not cause a segment fault', function(done) {
      var expectedKey = new Buffer('822285ee315d2b04');
      var expectedValue = new Buffer('ec65d632d9168c33350ed31a30848d01e95172931e90984c218ef6b08c1fa90a', 'hex');
      var env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 12,
        mapSize: 64 * 1024 * 1024
      });
      var dbi = env.openDbi({
        name: 'testfree',
        create: true
      });
      var txn = env.beginTxn();
      txn.putBinary(dbi, expectedKey, expectedValue);
      txn.commit();
      var txn2 = env.beginTxn();
      var cursor = new lmdb.Cursor(txn2, dbi);
      var key;
      var value;
      cursor.goToFirst();
      cursor.getCurrentBinary(function(returnKey, returnValue) {
        key = returnKey;
        value = returnValue;
      });
      cursor.close();
      txn2.abort();
      dbi.close();
      env.close();
      key.should.deep.equal(expectedKey);
      value.compare(expectedValue).should.equal(0);
      done();
    });
  });
  describe('Type Conversion', function() {
    var env;
    var dbi;
    var expectedKey = new Buffer('822285ee315d2b04', 'hex');
    var expectedValue = new Buffer('ec65d632d9168c33350ed31a30848d01e95172931e90984c218ef6b08c1fa90a', 'hex');
    before(function() {
      env = new lmdb.Env();
      env.open({
        path: testDirPath,
        maxDbs: 12,
        mapSize: 64 * 1024 * 1024
      });
      dbi = env.openDbi({
        name: 'testkeys',
        create: true
      });
      var txn = env.beginTxn();
      txn.putBinary(dbi, expectedKey, expectedValue);
      txn.commit();
    });
    after(function() {
      dbi.close();
      env.close();
    });
    it('will be able to convert key to buffer', function(done) {
      var txn = env.beginTxn();
      var cursor = new lmdb.Cursor(txn, dbi);
      cursor.goToFirst();
      cursor.getCurrentBinary(function(key, value) {
        var keyBuffer = new Buffer(key, 'hex');
        cursor.close();
        txn.abort();
        keyBuffer.compare(expectedKey).should.equal(0);
        value.compare(expectedValue).should.equal(0);
        done();
      });
    });
  });
});
