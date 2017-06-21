var lmdb, env, dbi;

lmdb = require('..');
//lmdb = require('node-lmdb');
env = new lmdb.Env();
env.open({
    // Path to the environment
    path: "./testdata",
    // Maximum number of databases
    maxDbs: 10
});

try {
    // If the database exists, drop it
    dbi = env.openDbi({
        name: "dupsort-delete",
        dupSort: true
    });
    dbi.drop();
}
catch (err) {}
dbi = env.openDbi({
    name: "dupsort-delete",
    create: true,
    keyIsUint32: true,
    dupSort: true,
    dupFixed: true,
    integerDup: true
});

console.log("ensured database is empty");

var printFunc = function(key, data) {
   console.log('  key:', key + ' => ' + data);
};

var showDBcontents = function(){
    const txn2 = env.beginTxn({readOnly:true});
    cursor = new lmdb.Cursor(txn2, dbi);

    let i = 0;
    console.log('======================================');
    console.log('Table: dupsort-delete');
    console.log('--------------------------------------');
    for (let found = cursor.goToFirst(); found; found = cursor.goToNext()) {
        i++;
        cursor.getCurrentNumber(printFunc);
    }
    console.log('record count = ' + i);
    console.log('======================================');


    // Close cursor
    cursor.close();

    // abort transaction
    txn2.abort();
};

var txn;

txn = env.beginTxn();
txn.putNumber(dbi, 100, 1);
txn.putNumber(dbi, 100, 2);
txn.putNumber(dbi, 100, 3);
txn.putNumber(dbi, 100, 4);
txn.putNumber(dbi, 101, 1);
txn.putNumber(dbi, 101, 2);
txn.putNumber(dbi, 101, 3);
txn.putNumber(dbi, 101, 4);
txn.putNumber(dbi, 102, 1);
txn.putNumber(dbi, 102, 2);
txn.putNumber(dbi, 102, 3);
txn.putNumber(dbi, 102, 4);

txn.commit();

console.log("wrote test values");
showDBcontents();


console.log("begin - deleting duplicate values");
txn = env.beginTxn();
txn.del(dbi, 101, 2);
txn.del(dbi, 101, 4);
txn.del(dbi, 102, 1);
txn.del(dbi, 102, 3);
txn.commit();

console.log("end - deleted some duplicate values");
showDBcontents();


dbi.close();
env.close();
