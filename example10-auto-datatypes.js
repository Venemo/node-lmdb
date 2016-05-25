
var lmdb = require('./build/Release/node-lmdb');
var env = new lmdb.Env();
env.open({
    // Path to the environment
    path: "./testdata",
    // Maximum number of databases
    maxDbs: 10
});
var dbi = env.openDbi({
   name: "mydb2",
   create: true
});

// Create transaction
var txn = env.beginTxn();

// Example for getting/putting/deleting string data
// ----------
var stringData = txn.get(dbi, "key1");
// Print the string
console.log("string data: ", stringData, typeof stringData);
// Toggle the value
if (stringData === null)
    txn.put(dbi, "key1", "Hello world!");
else
    txn.del(dbi, "key1");

// Example for getting/putting/deleting binary data
// ----------
var binaryData = txn.get(dbi, "key2");
// Print the string representation of the binary
console.log("binary data: ", binaryData, typeof binaryData);
// Toggle the value
if (binaryData === null) {
    //var buffer = new Buffer("Hey my friend");
    var buffer = Buffer.from([1, 2, 3]);
    txn.put(dbi, "key2", buffer);
}
else {
    txn.del(dbi, "key2");
}


// Example for getting/putting/deleting number data
// ----------
var numberData = txn.get(dbi, "key3");
// Print the number
console.log("number data: ", numberData, typeof numberData);
// Toggle the value
if (numberData === null)
    txn.put(dbi, "key3", 42);
else
    txn.del(dbi, "key3");

// Example for getting/putting/deleting boolean data
// ----------
var booleanData = txn.get(dbi, "key4");
// Print the boolean
console.log("boolean data: ", booleanData, typeof booleanData);
// Toggle the value
if (booleanData === null)
    txn.put(dbi, "key4", true);
else
    txn.del(dbi, "key4");

// Example for using integer key
// ----------
var data = txn.get(dbi, "key5");
console.log("integer key value: ", data, typeof data);
if (data === null)
    txn.put(dbi, "key5", "Hello worllld!");
else
    txn.del(dbi, "key5");

// Example for getting/putting/deleting object data
// ----------
try {
    var objectData = txn.get(dbi, "key6");
    // Print the boolean
    console.log("object data: ", objectData, typeof objectData);
    // Toggle the value
    if (objectData === null)
        txn.put(dbi, "key6", {a: 1, b: "abcde"});
    else
        txn.del(dbi, "key6");
} catch (e) {
    console.log(e.message);
}

// Example for getting/putting/deleting array data
// ----------
try {
    var arrayData = txn.get(dbi, "key7");
    // Print the boolean
    console.log("array data: ", arrayData, typeof arrayData);
    // Toggle the value
    if (arrayData === null)
        txn.put(dbi, "key7", [1, 2, 3, 4, 5]);
    else
        txn.del(dbi, "key7");
} catch (e) {
    console.log(e.message);
}

console.log("");
console.log("Run this example again to see the alterations on the database!");

// Commit transaction
txn.commit();

dbi.close();
env.close();

