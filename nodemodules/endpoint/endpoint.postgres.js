var pkt = require('./packet.js');
var bigint = require('./bigint.js');
var common = require('./common.js');
var naming = require('./naming.js');
var assert = require('assert');
var eq = require('deep-equal-ident');
var schema = require('./endpoint-schema.js');
var Q = require('q');
var largepacket = require('./largepacket.js');
var Path = require('path');
var util = require('util');
const { Pool, Client } = require('pg');

const dotenv = require('dotenv');
dotenv.config();

const pg_host = process.env.HOST;
const pg_password = process.env.PASSWORD;
const pg_database = process.env.DATABASE;
const pg_port = process.env.PORT;
const pg_user = process.env.USER;

const connectionString = process.env.DATABASE_URL || 'postgresql://' + pg_user + ':' + pg_password + '@' + pg_host + ':' + pg_port + '/' + pg_database;

function isSrcDstPair(x) {
    if (typeof x == 'object') {
        return x.hasOwnProperty('src') && x.hasOwnProperty('dst');
    }
    return false;
}

function compareSrcDstPair(a, b) {
    if (a.src == b.src) {
        return a.dst < b.dst;
    } else {
        return a.src < b.src;
    }
}

function eqSrcDstPair(a, b) {
    return a.src == b.src && a.dst == b.dst;
}

function isSorted(X) {
    for (var i = 0; i < X.length - 1; i++) {
        if (!compareSrcDstPair(X[i], X[i + 1])) {
            return false;
        }
    }
    return true;
}

function srcDstPairUnion(A, B) {
    var result = [];
    assert(A instanceof Array); assert(B instanceof Array);
    while (A.length > 0 && B.length > 0) {
        if (eqSrcDstPair(A[0], B[0])) {
            result.push(A[0]);
            A = A.slice(1);
            B = B.slice(1);
        } else if (compareSrcDstPair(A[0], B[0])) {
            result.push(A[0]);
            A = A.slice(1);
        } else {
            result.push(B[0]);
            B = B.slice(1);
        }
        assert(A instanceof Array); assert(B instanceof Array);
    }
    if (A.length > 0) {
        return result.concat(A);
    } else {
        return result.concat(B);
    }
}

function filterByName(pairs, name) {
    pairs.filter(function (p) { return p.src == name || p.dst == name; });
}

function srcDstPairIntersection(A, B) {
    var result = [];
    assert(A instanceof Array); assert(B instanceof Array);
    while (A.length > 0 && B.length > 0) {
        if (eqSrcDstPair(A[0], B[0])) {
            result.push(A[0]);
            A = A.slice(1);
            B = B.slice(1);
        } else if (compareSrcDstPair(A[0], B[0])) {
            A = A.slice(1);
        } else {
            B = B.slice(1);
        }
        assert(A instanceof Array); assert(B instanceof Array);
    }
    return result;
}

function srcDstPairDifference(A, B) {
    var result = [];
    assert(A instanceof Array); assert(B instanceof Array);
    while (A.length > 0 && B.length > 0) {
        if (eqSrcDstPair(A[0], B[0])) {
            A = A.slice(1);
            B = B.slice(1);
        } else if (compareSrcDstPair(A[0], B[0])) {
            result.push(A[0]);
            A = A.slice(1);
        } else {
            B = B.slice(1);
        }
        assert(A instanceof Array); assert(B instanceof Array);
    }
    return result;
}

var fullschema = "CREATE TABLE IF NOT EXISTS packets (boxId TEXT, src TEXT, dst TEXT, \
    seqNumber TEXT, label INT, data BYTEA, PRIMARY KEY(boxId, src, dst, seqNumber)); \
    CREATE TABLE IF NOT EXISTS lowerBounds (boxId TEXT, src TEXT, dst TEXT, lowerBound TEXT, PRIMARY KEY(boxId, src, dst));";

function beginTransaction(db, cb) {

    db.db.query('BEGIN', (err) => {
        if (err) {
            cb(err, null);
        } else {
            cb(null, db);
        }
    });
}


function commit(db, cb) {
    db.db.query('COMMIT', (err) => {
        if (err) {
            cb(err, null);
        } else {
            cb(null, db);
        }
    });
}

function rollback(db, cb) {
    db.db.query('ROLLBACK', (err) => {
        if (err) {
            cb(err, null);
        } else {
            cb(null, db);
        }
    });
}


function withTransaction(db, cbTransaction, cbDone) {
    beginTransaction(db, function (err, T) {
        if (err) {
            cbDone(err);
        } else {
            //cbTransaction(T, function(err, results) {
            cbTransaction(db, function (err, results) {

                // Called from rollback or commit
                var onFinish = function (e) {
                    var totalErr = e || err;
                    if (totalErr) {
                        cbDone(totalErr);
                    } else {
                        cbDone(null, results);
                    }
                };

                if (err) {
                    //T.rollback(onFinish);
                    rollback(db, onFinish);
                } else {
                    //T.commit(onFinish);
                    commit(db, onFinish);
                }
            });
        }
    });
}

function createAllTables(db, cb) {
    db.db.query(fullschema, cb);
}

function dropTables(db, cb) {
    db.db.query("DELETE FROM packets WHERE boxId = $1;", [db.boxId], 
      function(err) {
        if (err) {
          cb(err);
          return;
        }
        db.db.query("DELETE FROM lowerBounds WHERE boxId = $1;", [db.boxId], cb);
      }
    );
}


function getUniqueSrcDstPairs(db, tableName, cb) {
    db.db.query('SELECT DISTINCT src,dst FROM ' + tableName + ' WHERE boxId=$1 ORDER BY src, dst', [db.boxId],
   (err, res) => {
       if (err) { cb(err); }
       else { cb(null, res.rows); }
   });
}

let dbPool;
function openDBWithFilename(dbFilename, cb) {
    if (!dbPool) {
        dbPool = new Pool({
            connectionString: connectionString,
        });
    }
    const db = {
        db: dbPool, boxId: dbFilename
    };
    createAllTables(db, function (err) {
        if (err) {
            console.warn("openWithFilename err: ", err)
            cb(err);
        } else {
            cb(undefined, db);
        }
    });
}

function Endpoint(filename, name, db) {
    this.db = db;
    this.dbFilename = filename;
    this.name = name;
    this.packetHandlers = [largepacket.largePacketHandler];
    this.isLeaf = true;
    this.settings = {
        mtu: 100000 // in bytes
    };
}

function tryMakeEndpoint(filename, name, cb) {
    if (!(common.isString(filename))) {
        cb(new Error('Invalid filename to tryMakeEndpoint: ' + filename));
    } else if (!common.isIdentifier(name)) {
        cb(new Error('Invalid name to tryMakeEndpoint: ' + name));
    } else {
        openDBWithFilename(filename, function (err, db) {
            if (err) {
                console.log(filename + ': error: ' + err);
                cb(err);
            } else {
                cb(null, new Endpoint(filename, name, db));
            }
        });
    }
}

Endpoint.prototype.reset = function (cb) {
    beginTransaction(this.db, function (err, T) {
        if (err) {
            cb(err);
        } else {
            dropTables(T, function (err) {
                if (err) {
                    cb(err);
                } else {
                    commit(T, function (err2) {
                        cb(err || err2);
                    });
                }
            });
        }
    });
}

function tryMakeAndResetEndpoint(filename, name, cb) {
    tryMakeEndpoint(filename, name, function (err, ep) {
        if (err) {
            cb(err);
        } else {
            ep.reset(function (err) {
                if (err) {
                    cb(err);
                } else {
                    cb(null, ep);
                }
            });
        }
    });
}

function getLowerBoundFromTable(db, src, dst, cb) {
    const selValues = [db.boxId, src, dst];
    db.db.query(
        'SELECT lowerbound as "lowerBound" FROM lowerBounds WHERE boxID = $1 AND src = $2 AND dst = $3',
        selValues, function (err, res) {
            if (err) {
                cb(err);
            } else {
                if (res.rows[0]) {
                    cb(null, res.rows[0].lowerBound);
                } else {
                    cb();
                }
            }
        });
}

function getFirstPacketIndex(db, src, dst, cb) {
    const selValues = [db.boxId, src, dst]
    db.db.query(
        'SELECT seqnumber as "seqNumber" FROM packets WHERE boxId = $1 AND src = $2 AND dst = $3 ORDER BY seqnumber',
        selValues,
        function (err, res) {
            if (err) {
                cb(err);
            } else {
                if (res.rows[0]) {
                    cb(null, res.rows[0].seqNumber);
                } else {
                    cb();
                }
            }
        });
}

function computeTheLowerBound(lowerBound0, lowerBound1) {
    lowerBound0 = lowerBound0 || bigint.zero();
    lowerBound1 = lowerBound1 || bigint.zero();
    return (lowerBound0 < lowerBound1 ? lowerBound1 : lowerBound0);
}

/*

  Try in this order:

  1. Read it from the table lowerBounds
  2. Try to retrieve the first packet index
  3. Return 0

*/
function getLowerBound(T, src, dst, cb) {
    if (!(common.isIdentifier(src) && common.isIdentifier(dst))) {
        cb(new Error('Invalid identifiers in getLowerBound'));
    } else {
        getLowerBoundFromTable(T, src, dst, function (err, lowerBound0) {
            if (err) {
                cb(err);
            } else {
                getFirstPacketIndex(T, src, dst, function (err, lowerBound1) {
                    if (err) {
                        cb(err);
                    } else {
                        const lb = computeTheLowerBound(lowerBound0, lowerBound1);
                        cb(null, lb);
                    }
                });
            }
        });
    }
}

Endpoint.prototype.getLowerBound = function (src, dst, cb) {
    withTransaction(
        this.db,
        function (T, cb) {
            getLowerBound(T, src, dst, cb);
        }, cb);
}

function getPacket(db, src, dst, seqNumber, cb) {
    const selValues = [db.boxId, src, dst, seqNumber]
    db.db.query(
        'SELECT src, dst, seqnumber as "seqNumber", label, data FROM packets WHERE boxId = $1 AND src = $2 AND dst = $3 AND seqNumber = $4 LIMIT 1',
        selValues, function (err, res) {
            if (err) {
                cb(err);
            } else {
                cb(null, res.rows[0]);
            }
        });
}


Endpoint.prototype.getPacket = function (src, dst, seqNumber, cb) {
    getPacket(this.db, src, dst, seqNumber, cb);
}


/*
function getLastPacket(db, src, dst, cb) {
    const selValues = [db.boxId, src, dst]
    db.db.query('SELECT src, dst, seqnumber, label, data FROM packets WHERE boxId = $1 AND src = $2 AND dst = $3 ORDER BY seqNumber DESC',
        selValues, (err, res);
}
*/


function getUpperBound(db, src, dst, cb) {
    const selValues = [db.boxId, src, dst]
    db.db.query('SELECT seqnumber as "seqNumber" FROM packets WHERE boxId = $1 AND src = $2 AND dst = $3 ORDER BY seqNumber DESC',
        selValues, function (err, res) {
            if (err) {
                cb(err);
            } else if (res.rows[0]) {
                cb(null, bigint.inc(res.rows[0].seqNumber));
            } else {
                getLowerBound(db, src, dst, cb);
            }
        });
}


Endpoint.prototype.getUpperBound = function (src, dst, cb) {
    // First try the lastPacket+1
    // Then the lower bound
    withTransaction(this.db, function (T, cb) {
        getUpperBound(T, src, dst, cb);
    }, cb);
}

// lower <= seqNumber < upper 
Endpoint.prototype.getPacketBounds = function (src, dst, cb) {
    withTransaction(this.db, function (T, cb) {
        getFirstPacketIndex(T, src, dst, function (err, lb) {
            if (err) {
                cb(err);
            } else if (!lb) {
                cb();
            } else {
                getUpperBound(T, src, dst, function (err, ub) {
                    if (err) {
                        cb(err);
                    } else if (!ub) {
                        cb();
                    } else {
                        cb(null, { lower: lb, upper: ub });
                    }
                });
            }
        });
    }, cb);
}

function ensureNumberOr0(x) {
    return x == null ? 0 : x;
}


function getSizeOfRange(db, src, dst, lower, upper, cb) {
    const selValues = [db.boxId, src, dst, lower, upper]
    db.db.query(
        'SELECT sum(length(data)) AS size FROM packets WHERE boxId = $1 AND src=$2 AND dst=$3 AND $4 <= seqnumber AND seqnumber < $5',
        selValues, function (err, res) {
            if (err) {
                cb(err);
            } else if (res.rows == null) {
                cb(null, 0);
            } else {
                cb(null, ensureNumberOr0(res.rows[0].size));
            }
        });
}

Endpoint.prototype.getSizeOfRange = function (src, dst, lower, upper, cb) {
    getSizeOfRange(this.db, src, dst, lower, upper, cb);
}

function getRangeSizesSub(db, rangeSpecs, result, cb) {
    if (rangeSpecs.length == 0) {
        cb(null, result);
    } else {
        var spec = rangeSpecs[0];
        getSizeOfRange(
            db, spec.src, spec.dst, spec.lower, spec.upper,
            function (err, s) {
                if (err) {
                    cb(err);
                } else {
                    result[result.length - rangeSpecs.length] = {
                        src: spec.src,
                        dst: spec.dst,
                        lower: spec.lower,
                        upper: spec.upper,
                        size: s
                    };
                    getRangeSizesSub(db, rangeSpecs.slice(1), result, cb);
                }
            });
    }
}


function getRangeSizes(db, rangeSpecs, cb) {
    var result = new Array(rangeSpecs.length);
    getRangeSizesSub(db, rangeSpecs, result, cb);
}

Endpoint.prototype.getRangeSizes = function (rangeSpecs, cb) {
    withTransaction(this.db, function (T, cb) {
        getRangeSizes(T, rangeSpecs, cb);
    }, cb);
}

function getNextSeqNumber(T, src, dst, cb) {
    getUpperBound(T, src, dst, function (err, ub) {
        if (err) {
            cb(err);
        } else {
            cb(null, ub == bigint.zero() ? bigint.makeFromTime() : ub);
        }
    });
}

Endpoint.prototype.getNextSeqNumber = function (src, dst, cb) {
    getNextSeqNumber(this.db, src, dst, cb);
}


function storePacket(db, packet, cb) {
    const packetValues = [db.boxId, packet.src, packet.dst, packet.seqNumber,
    packet.label, packet.data]
    db.db.query(
        'INSERT INTO packets (boxId, src, dst, seqNumber, label, data) VALUES ($1, $2, $3, $4, $5, $6)',
        packetValues, cb);
}

// Used by Endpoint.prototype.sendSimplePacketBatch
function sendSimplePacketBatch(T, src, sent, array, generator, cb) {
    if (array.length == 0) {
        cb(null, sent);
    } else {
        var nextPacket = generator(sent, array[0])
        if (nextPacket.dst == null || nextPacket.label == null || nextPacket.data == null) {
            cb(new Error('Incomplete packet information returned from generator'));
        } else {
            getNextSeqNumber(T, src, nextPacket.dst, function (err, seqNumber) {
                if (err) {
                    cb(err);
                } else {
                    nextPacket.seqNumber = seqNumber;
                    nextPacket.src = src;
                    storePacket(T, nextPacket,
                        function (err) {
                            if (err) {
                                cb(err, sent);
                            } else {
                                sent.push(nextPacket);
                                sendSimplePacketBatch(
                                    T, src, sent, array.slice(1), generator, cb);
                            }
                        });
                }
            });
        }
    }
}

/*

Send multiple packets in one transaction, with contiguous
sequence numbers:

Explanation:

  * 'array' is any type of array over which we iterator
  * 'generator' is a function with arguments
    (sent, e), where 'sent' is an array of all packets
    sent so far,  and 'e' is the next element in the array over which
    we are iterating. The generator returns a map with entries 
    (dst, label, data) for the packet to be sent next.
  * 'cb' is called once everything is done, with the first argument
    being an error, or the first argument being null and the second
    argument being all the packets that were sent.

*/
Endpoint.prototype.sendSimplePacketBatch = function (array, generator, cb) {
    assert(typeof cb == 'function');
    if (!(array instanceof Array)) {
        cb(new Error('Not an array'));
    }
    if (!(typeof generator == 'function')) {
        cb(new Error('Generator is not a function'));
    }
    var self = this;
    withTransaction(this.db, function (T, cb) {
        sendSimplePacketBatch(T, self.name, [], array, generator, cb);
    }, cb);
};

Endpoint.prototype.sendSimplePacketAndReturn = function (dst, label, data, cb) {
    this.sendSimplePacketBatch([null], function () {
        return {
            dst: dst,
            label: label,
            data: data
        };
    }, function (err, sent) {
        if (err) {
            cb(err);
        } else if (!(sent instanceof Array)) {
            cb(new Error('sent is not an array'));
        } else if (sent.length != 1) {
            cb(new Error('sent is not exactly one element'));
        } else {
            cb(null, sent[0]);
        }
    });
}


// Usually we always pass data to
function tryToConvertToBuffer(x) {
    if (x instanceof Buffer) {
        return x;
    }
    try {
        if (x.data && x.type == 'Buffer') {
            return new Buffer(x.data);
        }
    } catch (e) { }
    return x;
}

Endpoint.prototype.sendPacketAndReturn = function (dst, label, data0, cb) {
    var data = tryToConvertToBuffer(data0);
    assert(this.settings);
    if (data instanceof Buffer) {
        if (data.length <= this.settings.mtu) {
            this.sendSimplePacketAndReturn(dst, label, data, cb);
        } else {
            largepacket.sendPacket(this, dst, label, data, this.settings, cb);
        }
    } else {
        console.log('THIS IS NOT A BUFFER: %j', data);
        console.log(data);
        console.log('Is it a buffer? ' + (data instanceof Buffer ? 'YES' : 'NO'));
        cb(new Error('Expected a buffer, but got of type ' + typeof data + ': ' + data));
    }
}

Endpoint.prototype.sendPacket = function (dst, label, data, cb) {
    assert(this.settings);
    this.sendPacketAndReturn(dst, label, data, function (err, p) {
        cb(err);
    });
}

function setLowerBoundInTable(db, src, dst, lowerBound, cb) {
    const insValues = [db.boxId, src, dst, lowerBound]
    db.db.query(
        `INSERT INTO lowerBounds (boxId, src, dst, lowerBound) VALUES ($1, $2, $3, $4)
         ON CONFLICT(boxId, src, dst)
         DO UPDATE SET lowerBound = $4`,
        insValues, cb);
}

var packetsToKeep = [common.firstPacket, common.remainingPacket];

var protectPacketSqlCmd = packetsToKeep
    .map(function (label) {
        assert(typeof label == 'number');
        return ' AND label <> ' + label;
    }).join('');


function removeObsoletePackets(ep, db, src, dst, lowerBound, cb) {
    const delValues = [db.boxId, src, dst, lowerBound]
    db.db.query(
        'DELETE FROM packets WHERE boxId=$1 AND src = $2 AND dst = $3 AND seqNumber < $4'
        + (ep.name == dst ? protectPacketSqlCmd : ''),
        delValues, cb);
}


Endpoint.prototype.getTotalPacketCount = function (cb) {
    var stmnt = 'SELECT count(*) AS cnt FROM packets WHERE boxId=$1';
    this.db.db.query(
        stmnt, [this.db.boxId], function (err, res) {
            if (err == undefined) {
                cb(err, res.rows[0].cnt);
            } else {
                cb(err);
            }
        });
}

function updateLowerBound(ep, db, src, dst, lowerBound, cb) {
    getLowerBound(db, src, dst, function (err, currentLowerBound) {
        if (err) {
            cb(err);
        } else {
            if (!lowerBound) {
                cb(null, currentLowerBound);
            } else {
                if (currentLowerBound < lowerBound) {
                    setLowerBoundInTable(db, src, dst, lowerBound, function (err) {
                        if (err) {
                            cb(err);
                        } else {
                            removeObsoletePackets(ep, db, src, dst, lowerBound, function (err) {
                                if (err) {
                                    cb(err);
                                } else {
                                    cb(null, lowerBound);
                                }
                            });
                        }
                    });
                } else {
                    cb(null, currentLowerBound);
                }
            }
        }
    });
}

function setLowerBound(ep, db, src, dst, lowerBound, cb) {
    updateLowerBound(ep, db, src, dst, lowerBound, function (err, lb) {
        cb(err);
    });
}

Endpoint.prototype.updateLowerBound = function (src, dst, lb, cb) {
    var self = this;
    withTransaction(this.db, function (T, cb) {
        updateLowerBound(self, T, src, dst, lb, cb);
    }, function (err, lb) {
        if (err) {
            cb(err);
        } else if (!common.isCounter(lb)) {
            cb(new Error('Invalid lower bound: ' + lb));
        } else {
            cb(err, lb);
        }
    });
}

Endpoint.prototype.getSrcDstPairs = function (cb) {
    getUniqueSrcDstPairs(this.db, 'packets', cb);
}

function getPerPairData(T, pairs, fun, field, cb) {
    common.withException(function () {
        var counter = 0;
        var results = new common.ResultArray(pairs.length, cb);
        for (var i = 0; i < pairs.length; i++) {
            var pair = pairs[i];
            fun(T, pair.src, pair.dst, results.makeSetter(i));
        }
    }, cb);
}


Endpoint.prototype.getLowerBounds = function (pairs, cb) {
    withTransaction(this.db, function (T, cb) {
        getPerPairData(T, pairs, getLowerBound, 'lb', cb);
    }, cb);
}

Endpoint.prototype.updateLowerBounds = function (pairs, cb) {
    const updateLowerBoundP = (p) => {
        return new Promise((resolve, reject) => {
            this.updateLowerBound(p.src, p.dst, p.lb, (err, newlb) => {
                if (err) reject(err);
                else resolve(newlb);
            });
        });
    };

    (async () => {
        try {
            const lbs = [];
            for (let p of pairs) {
                lbs.push(await updateLowerBoundP(p));
            }
            cb(null, lbs);
        } catch(err) {
            cb(err);
        }
    })();
}

Endpoint.prototype.getUpperBounds = function (pairs, cb) {
    withTransaction(this.db, function (T, cb) {
        getPerPairData(T, pairs, getUpperBound, 'ub', cb);
    }, cb);
}


var packetHandlerParamNames = ['endpoint', 'packet'];
Endpoint.prototype.addPacketHandler = function (handler) {
    var paramNames = common.getParamNames(handler);
    assert(paramNames.length == packetHandlerParamNames.length);
    for (var i = 0; i < paramNames.length; i++) {
        if (paramNames[i] != packetHandlerParamNames[i]) {
            throw new Error('Expected names ' + packetHandlerParamNames + ' but got ' + paramNames);
        }
    }
    this.packetHandlers.push(handler);
}

Endpoint.prototype.setIsLeaf = function (x) {
    this.isLeaf = x;
}

Endpoint.prototype.callPacketHandlers = function (p) {
    for (var i = 0; i < this.packetHandlers.length; i++) {
        this.packetHandlers[i](this, p);
    }
}

Endpoint.prototype.putPacket = function (packet, cb) {
    var self = this;
    withTransaction(this.db, function (T, cb) {
        getLowerBound(T, packet.src, packet.dst, function (err, lb) {
            if (err) {
                cb(err);
            } else if (packet.seqNumber < lb) {
                cb();
            } else {
                if (self.name == packet.dst) {
                    try {
                        self.callPacketHandlers(packet);
                        setLowerBound(self, T, packet.src, packet.dst, bigint.inc(packet.seqNumber), cb);
                    } catch (e) {
                        console.log('Catched an exception in packetHandler: ');
                        console.log(e.message);
                        console.log(e.stack);
                        cb(e);
                    }
                } else {
                    getPacket(T, packet.src, packet.dst, packet.seqNumber, function (err, packet2) {
                        if (err) {
                            cb(err);
                        } else {
                            if (packet2) {
                                if (eq(packet, packet2)) {
                                    cb();
                                } else {
                                    console.warn('Packet 1:', packet, ', packet2:', packet2);
                                    cb(new Error('A different packet has already been delivered'));
                                }
                            } else {
                                storePacket(T, packet, cb);
                            }
                        }
                    });
                }
            }
        });
    }, cb);
}

//function getAllFromTable(db, tableName, cb) { db.db.query('SELECT * FROM ' + tableName + ';', cb); }

Endpoint.prototype.disp = function (cb) {
    var self = this;
    getAllFromTable(self.db, 'packets', function (err, packets) {
        if (err) {
            cb(err);
        } else {
            getAllFromTable(self.db, 'lowerBounds', function (err, lowerBounds) {
                if (err) {
                    cb(err);
                } else {
                    console.log('DISPLAY ' + self.name);
                    console.log('  Packets');
                    for (var i = 0; i < packets.length; i++) {
                        var p = packets[i];
                        p.data = '(hidden)';
                        console.log('    %j', p);
                    }
                    console.log('  Lower bounds');
                    for (var i = 0; i < lowerBounds.length; i++) {
                        var p = lowerBounds[i];
                        console.log('    %j', p);
                    }
                    console.log('DONE DISPLAYING');
                    cb();
                }
            });
        }
    });
}

Endpoint.prototype.close = function (cb) {
    this.db.close(cb);
    this.db = null;
}

Endpoint.prototype.open = function (cb) {
    var self = this;
    if (!self.db) {
        openDBWithFilename(this.dbFilename, function (err, db) {
            self.db = db;
            cb(err);
        });
    } else {
        cb();
    }
}

Endpoint.prototype.makeVerbose = function () {
    schema.makeVerbose(this);
}

function tryMakeEndpointFromFilename(dbFilename, cb) {
    var endpointName = naming.getEndpointNameFromFilename(dbFilename);
    if (endpointName) {
        tryMakeEndpoint(dbFilename, endpointName, cb);
    } else {
        cb(new Error('Unable to extract endpointname from filename ' + dbFilename));
    }
}

function isEndpoint(x) {
    if (typeof x == 'object') {
        return x.constructor.name == 'Endpoint';
    }
    return false;
}

function withEP(ep, epOperation, done) {
    ep.open(function (err) {
        if (err) {
            console.log('Failed to open endpoint');
            done(err);
        } else {
            epOperation(ep, function (err) {
                ep.close(function (err2) {
                    done(err || err2);
                });
            });
        }
    });
}

//module.exports.getAllFromTable = getAllFromTable;
module.exports.Endpoint = Endpoint;
module.exports.isEndpoint = isEndpoint;
module.exports.tryMakeEndpoint = tryMakeEndpoint;
module.exports.tryMakeAndResetEndpoint = tryMakeAndResetEndpoint;
module.exports.srcDstPairUnion = srcDstPairUnion;
module.exports.srcDstPairIntersection = srcDstPairIntersection;
module.exports.srcDstPairDifference = srcDstPairDifference;
module.exports.filterByName = filterByName;
module.exports.tryMakeEndpointFromFilename = tryMakeEndpointFromFilename;
module.exports.withEP = withEP;
module.exports.storePacket = storePacket;
module.exports.withTransaction = withTransaction;
