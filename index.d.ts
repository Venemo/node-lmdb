declare module "node-lmdb" {
  type Key = string | number | Buffer;
  type Value = string | number | Buffer | boolean;

  type KeyType =
    | {
        /** if true, keys are treated as 32-bit unsigned integers */
        keyIsUint32?: boolean;
      }
    | {
        /** if true, keys are treated as Buffers */
        keyIsBuffer?: boolean;
      }
    | {
        /** if true, keys are treated as strings */
        keyIsString?: boolean;
      };

  type PutOptions = {
    noDupData?: boolean;
    noOverwrite?: boolean;
    append?: boolean;
    appendDup?: boolean;
  } & KeyType;

  interface Stat {
    pageSize: number;
    treeDepth: number;
    treeBranchPageCount: number;
    treeLeafPageCount: number;
    entryCount: number;
    overflowPages: number;
  }

  type CursorCallback<T> = (k: Key, v: T) => void;

  enum BatchResult {
    SUCCESS = 0,
    CONDITION_NOT_MET = 1,
    NOT_FOUND = 2,
  }

  /**
   * Object argument for Env.batchWrite()
   */
  interface BatchOperation {
    /** the database instance to target for the operation */
    db: Dbi;
    /** the key to target for the operation */
    key: Key;
    /** If null, treat as a DELETE operation */
    value?: Value;
    /** If provided, ifValue must match the first X bytes of the stored value or the operation will be canceled */
    ifValue?: Value;
    /** If true, ifValue must match all bytes of the stored value or the operation will be canceled */
    ifExactMatch?: boolean;
    /** If provided, use this key to determine match for ifValue */
    ifKey?: Key;
    /** If provided, use this DB to determine match for ifValue */
    ifDB: Dbi;
  }

  /**
   * Array argument for Env.batchWrite()
   * @example [db: Dbi, key: Key] // DELETE operation
   * @example [db: Dbi, key: Key, value: Value] // PUT operation
   * @example [db: Dbi, key: Key, value: Value, ifValue: Value] // PUT IF operation
   */
  type BatchOperationArray =
    | [db: Dbi, key: Key]
    | [db: Dbi, key: Key, value: Value]
    | [db: Dbi, key: Key, value: Value, ifValue: Value];

  /**
   * Options for opening a database instance
   */
  type DbiOptions = {
    /** the name of the database (or null to use the unnamed database) */
    name?: string;
    /** if true, the database will be created if it doesn't exist */
    create?: boolean;
    /** keys are strings to be compared in reverse order */
    reverseKey?: boolean;
    /** if true, the database can hold multiple items with the same key */
    dupSort?: boolean;
    /** if dupSort is true, indicates that the data items are all the same size */
    dupFixed?: boolean;
    /** duplicate data items are also integers, and should be sorted as such */
    integerDup?: boolean;
    /** duplicate data items should be compared as strings in reverse order */
    reverseDup?: boolean;
    /** if a read/write transaction is currently open, pass it here */
    txn?: Txn;
  } & KeyType;

  interface EnvOptions {
    path?: string;
    mapSize?: number;
    maxDbs?: number;
  }

  interface TxnOptions {
    readOnly: boolean;
  }

  class Env {
    open(options: EnvOptions): void;

    /**
     * Open a database instance
     * @param {DbiOptions} options
     */
    openDbi(options: DbiOptions): Dbi;

    /**
     * Begin a transaction
     */
    beginTxn(options?: TxnOptions): Txn;

    /**
     * Detatch from the memory-mapped object retrieved with getStringUnsafe()
     * or getBinaryUnsafe(). This must be called after reading the object and
     * before it is accessed again, or V8 will crash.
     * @param buffer
     */
    detachBuffer(buffer: ArrayBufferLike): void;

    /**
     * Retrieve Environment statistics.
     */
    stat(): Stat;

    /**
     * When `batchWrite` is called, `node-ldmb` will asynchronously create a
     * new write transaction, execute all the operations in the provided
     * array, except for any conditional writes where the condition failed,
     * and commit the transaction, if there were no errors. For conditional
     * writes, if the condition did not match, the write will be skipped,
     * but the transaction will still be committed. However, if any errors
     * occur, the transaction will be aborted. This entire transaction will
     * be created by `node-lmdb` and executed in a separate thread. The
     * callback function will be called once the transaction is finished. It
     * is possible for an explicit write transaction in the main JS thread
     * to block or be blocked by the asynchronous transaction.
     * @param {Array} operations
     * @param {object} options
     * @param {Function} options.progress callback function for reporting
     *                                    progress on a batch operation.
     * @param callback
     */
    batchWrite(
      operations: (BatchOperation | BatchOperationArray)[],
      options?: PutOptions & {
        progress: (results: BatchResult[]) => void;
      },
      callback?: (err: Error, results: BatchResult[]) => void
    ): void;

    copy(
      path: string,
      compact?: boolean,
      callback?: (err: Error) => void
    ): void;

    /**
     * Close the environment
     */
    close(): void;
  }

  type DropOptions = { txn?: Txn; justFreePages: boolean };

  /**
   * Database Instance: represents a single K/V store.
   */
  type Dbi = {
    close(): void;
    drop(options?: DropOptions): void;
    stat(tx: Txn): Stat;
  };

  /**
   * Transaction (read-only or read-write)
   */
  type Txn = {
    getString(dbi: Dbi, key: Key, options?: KeyType): string;
    putString(dbi: Dbi, key: Key, value: string, options?: PutOptions): void;

    getBinary(dbi: Dbi, key: Key, options?: KeyType): Buffer;
    putBinary(dbi: Dbi, key: Key, value: Buffer, options?: PutOptions): void;

    getNumber(dbi: Dbi, key: Key, options?: KeyType): number;
    putNumber(dbi: Dbi, key: Key, value: number, options?: PutOptions): void;

    getBoolean(dbi: Dbi, key: Key, options?: KeyType): boolean;
    putBoolean(dbi: Dbi, key: Key, value: boolean, options?: PutOptions): void;

    del(dbi: Dbi, key: Key, options?: KeyType): void;

    /**
     * Retrieve a string using zero-copy semantics. Env.detachBuffer() must
     * be called on the underlying buffer after the data is accessed.
     */
    getStringUnsafe(dbi: Dbi, key: Key, options?: KeyType): string;

    /**
     * Retrieve a Buffer using zero-copy semantics. Env.detachBuffer() must
     * be called on the underlying buffer after the data is accessed.
     */
    getBinaryUnsafe(dbi: Dbi, key: Key, options?: KeyType): Buffer;

    /**
     * Commit and close the transaction
     */
    commit(): void;

    /**
     * Abort and close the transaction
     */
    abort(): void;

    /**
     * Abort a read-only transaction, but makes it renewable by calling
     * renew().
     */
    reset(): void;

    /**
     * Renew a read-only transaction after it has been reset.
     */
    renew(): void;
  };

  interface DelOptions {
    noDupData: boolean;
  }
  class Cursor<T extends Key = string> {
    constructor(txn: Txn, dbi: Dbi, keyType?: KeyType);

    goToFirst(options?: KeyType): T | null;
    goToLast(options?: KeyType): T | null;
    goToNext(options?: KeyType): T | null;
    goToPrev(options?: KeyType): T | null;
    goToKey(key: T, options?: KeyType): T | null;
    goToRange(key: T, options?: KeyType): T | null;

    goToFirstDup(options?: KeyType): T | null;
    goToLastDup(options?: KeyType): T | null;
    goToNextDup(options?: KeyType): T | null;
    goToPrevDup(options?: KeyType): T | null;
    goToDup(key: T, data: Value, options?: KeyType): T | null;
    goToDupRange(key: T, data: Value, options?: KeyType): T | null;

    getCurrentNumber(fn?: CursorCallback<number>): number | null;
    getCurrentBoolean(fn?: CursorCallback<boolean>): boolean | null;
    getCurrentString(fn?: CursorCallback<string>): string | null;
    getCurrentBinary(fn?: CursorCallback<Buffer>): Buffer | null;

    getCurrentStringUnsafe(fn?: CursorCallback<string>): string | null;
    getCurrentBinaryUnsafe(fn?: CursorCallback<Buffer>): Buffer | null;

    del(options?: DelOptions): void;

    close(): void;
  }
}
