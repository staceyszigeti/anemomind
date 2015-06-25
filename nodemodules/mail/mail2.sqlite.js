var TransactionDatabase = require("sqlite3-transactions").TransactionDatabase;
var sqlite3 = require('sqlite3').verbose();
var pkt = require('./packet.js');
var bigint = require('./bigint.js');
var common = require('./common.js');
var naming = require('./naming.js');
var assert = require('assert');
var eq = require('deep-equal-ident');
var schema = require('./endpoint-schema.js');
var Q = require('q');

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
    if (!compareSrcDstPair(X[i], X[i+1])) {
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
  pairs.filter(function(p) {return p.src == name || p.dst == name;});
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

var fullschema = "CREATE TABLE IF NOT EXISTS packets (src TEXT, dst TEXT, \
seqNumber TEXT, label INT, data BLOB, PRIMARY KEY(src, dst, seqNumber)); \
CREATE TABLE IF NOT EXISTS lowerBounds (src TEXT, dst TEXT, lowerBound TEXT, PRIMARY KEY(src, dst));";

function beginTransaction(db, cb) {
  //console.log("BEGIN TRANSACTION");
  //inTransaction = true;
  db.beginTransaction(cb);
}

function commit(T, cb) {
  //console.log("END TRANSACTION");
  //inTransaction = false;
  T.commit(cb);
}

function withTransaction(db, cbTransaction, cbDone) {
  beginTransaction(db, function(err, T) {
    if (err) {
      cbDone(err);
    } else {
      cbTransaction(T, function(err, results) {

        // Called from rollback or commit
        var onFinish = function(e) {
          var totalErr = e || err;
          if (totalErr) {
            cbDone(totalErr);
          } else {
            cbDone(null, results);
          }
        };
        
        if (err) {
          T.rollback(onFinish);
        } else {
          T.commit(onFinish);
        }
      });
    }
  });
}


function createAllTables(db, cb) {
  db.exec(fullschema, cb);
}

function dropTables(db, cb) {
  var names = ['packets', 'lowerBounds'];
  var query = '';
  for (var i = 0; i < names.length; i++) {
    query += 'DROP TABLE IF EXISTS ' + names[i] + ';';
  }
  db.exec(query, cb);
}


function getUniqueSrcDstPairs(db, tableName, cb) {
  db.all('SELECT DISTINCT src,dst FROM ' + tableName + ' ORDER BY src, dst', cb);
}

function openDBWithFilename(dbFilename, cb) {
  var db = new TransactionDatabase(
    new sqlite3.Database(
      dbFilename,
      function(err) {
	if (err != undefined) {
	  cb(err);
	}
      }
    )
  );
  createAllTables(db, function(err) {
    if (err) {
      cb(err);
    } else {
      cb(undefined, db);
    }
  });
}





function EndPoint(filename, name, db) {
  this.db = db;
  this.dbFilename = filename;
  this.name = name;
  this.packetHandlers = [];
  this.isLeaf = true;
}

function tryMakeEndPoint(filename, name, cb) {
  if (!(common.isString(filename) && common.isIdentifier(name))) {
    cb(new Error('Invalid input to tryMakeEndPoint'));
  } else {
    openDBWithFilename(filename, function(err, db) {
      if (err) {
        cb(err);
      } else {
        cb(null, new EndPoint(filename, name, db));
      }
    });
  }
}

EndPoint.prototype.reset = function(cb) {
  beginTransaction(this.db, function(err, T) {
    if (err) {
      cb(err);
    } else {
      dropTables(T, function(err) {
	if (err) {
	  cb(err);
	} else {
	  createAllTables(T, function(err) {
	    commit(T, function(err2) {
	      cb(err || err2);
	    });
	  });
	}
      });
    }
  });
}

function tryMakeAndResetEndPoint(filename, name, cb) {
  tryMakeEndPoint(filename, name, function(err, ep) {
    if (err) {
      cb(err);
    } else {
      ep.reset(function(err) {
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
  db.get(
    'SELECT lowerBound FROM lowerBounds WHERE src = ? AND dst = ?',
    src, dst, function(err, row) {
      if (err) {
        cb(err);
      } else {
        if (row) {
          cb(null, row.lowerBound);
        } else {
          cb();
        }
      }
    });
}

function getFirstPacketIndex(db, src, dst, cb) {
  db.get(
    'SELECT seqNumber FROM packets WHERE src = ? AND dst = ? ORDER BY seqNumber',
    src, dst,
    function(err, row) {
      if (err) {
        cb(err);
      } else {
        if (row) {
          cb(null, row.seqNumber);
        } else {
          cb();
        }
      }
    });
}


function computeTheLowerBound(lowerBound0, lowerBound1) {
  lowerBound0 = lowerBound0 || bigint.zero();
  lowerBound1 = lowerBound1 || bigint.zero();
  return (lowerBound0 < lowerBound1? lowerBound1 : lowerBound0);
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
    getLowerBoundFromTable(T, src, dst, function(err, lowerBound0) {
      if (err) {
        cb(err);
      } else {
        getFirstPacketIndex(T, src, dst, function(err, lowerBound1) {
          if (err) {
            cb(err);
          } else {
            cb(null, computeTheLowerBound(lowerBound0, lowerBound1));
          }
        });
      }
    });
  }
}

EndPoint.prototype.getLowerBound = function(src, dst, cb) {
  withTransaction(
    this.db,
    function(T, cb) {
      getLowerBound(T, src, dst, cb);
    }, cb);
}

function getPacket(db, src, dst, seqNumber, cb) {
  db.get(
    'SELECT * FROM packets WHERE src = ? AND dst = ? AND seqNumber = ?',
    src, dst, seqNumber, function(err, row) {
      if (err) {
        cb(err);
      } else {
        cb(null, row);
      }
    });
}

EndPoint.prototype.getPacket = function(src, dst, seqNumber, cb) {
  getPacket(this.db, src, dst, seqNumber, cb);
}

function getLastPacket(db, src, dst, cb) {
  db.get('SELECT * FROM packets WHERE src = ? AND dst = ? ORDER BY seqNumber DESC',
         src, dst, cb);
}

function getUpperBound(db, src, dst, cb) {
  db.get('SELECT seqNumber FROM packets WHERE src = ? AND dst = ? ORDER BY seqNumber DESC',
         src, dst, function(err, row) {
    if (err) {
      cb(err);
    } else if (row) {
      cb(null, bigint.inc(row.seqNumber));
    } else {
      getLowerBound(db, src, dst, cb);
    }
  });
}

EndPoint.prototype.getUpperBound = function(src, dst, cb) {
  // First try the lastPacket+1
  // Then the lower bound
  withTransaction(this.db, function(T, cb) {
    getUpperBound(T, src, dst, cb);
  }, cb);
}

function getNextSeqNumber(T, src, dst, cb) {
  getUpperBound(T, src, dst, function(err, ub) {
    if (err) {
      cb(err);
    } else {
      cb(null, ub == bigint.zero()? bigint.makeFromTime() : ub);
    }
  });
}

EndPoint.prototype.getNextSeqNumber = function(src, dst, cb) {
  getNextSeqNumber(this.db, src, dst, cb);
}


EndPoint.prototype.sendPacketAndReturn = function(dst, label, data, cb) {
  var self = this;
  var src = self.name;
  withTransaction(this.db, function(T, cb) {
    getNextSeqNumber(T, src, dst, function(err, seqNumber) {
      T.run(
        'INSERT INTO packets VALUES (?, ?, ?, ?, ?)',
        src, dst, seqNumber, label, data, function(err) {
          if (err) {
            cb(err);
          } else {
            cb(null, {
              src: src,
              dst: dst,
              label: label,
              seqNumber: seqNumber,
              data: data
            });
          }
        });
    });
  }, cb);
}

EndPoint.prototype.sendPacket = function(dst, label, data, cb) {
  this.sendPacketAndReturn(dst, label, data, function(err, p) {
    cb(err);
  });
}

function setLowerBoundInTable(db, src, dst, lowerBound, cb) {
  db.run(
    'INSERT OR REPLACE INTO lowerBounds VALUES (?, ?, ?)',
    src, dst, lowerBound, cb);          
}

function removeObsoletePackets(db, src, dst, lowerBound, cb) {
  db.run(
    'DELETE FROM packets WHERE src = ? AND dst = ? AND seqNumber < ?',
    src, dst, lowerBound, cb);
}

EndPoint.prototype.getTotalPacketCount = function(cb) {
  var query = 'SELECT count(*) FROM packets';
  this.db.get(
    query, function(err, row) {
      if (err == undefined) {
	cb(err, row['count(*)']);
      } else {
	cb(err);
      }
    });
}

function updateLowerBound(db, src, dst, lowerBound, cb) {
  getLowerBound(db, src, dst, function(err, currentLowerBound) {
    if (err) {
      cb(err);
    } else {
      if (!lowerBound) {
        cb(null, currentLowerBound);
      } else {
        if (currentLowerBound < lowerBound) {
          setLowerBoundInTable(db, src, dst, lowerBound, function(err) {
            if (err) {
              cb(err);
            } else {
              removeObsoletePackets(db, src, dst, lowerBound, function(err) {
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

function setLowerBound(db, src, dst, lowerBound, cb) {
  updateLowerBound(db, src, dst, lowerBound, function(err, lb) {
    cb(err);
  });
}

EndPoint.prototype.updateLowerBound = function(src, dst, lb, cb) {
  withTransaction(this.db, function(T, cb) {
    updateLowerBound(T, src, dst, lb, cb);
  }, function(err, lb) {
    if (err) {
      cb(err);
    } else if (!common.isCounter(lb)) {
      cb(new Error('Invalid lower bound: ' + lb));
    } else {
      cb(err, lb);
    }
  });
}

EndPoint.prototype.getSrcDstPairs = function(cb) {
  getUniqueSrcDstPairs(this.db, 'packets', cb);
}



function getPerPairData(T, pairs, fun, field, cb) {
  common.withException(function() {
    var counter = 0;
    var results = new common.ResultArray(pairs.length, cb);
    for (var i = 0; i < pairs.length; i++) {
      var pair = pairs[i];
      fun(T, pair.src, pair.dst, results.makeSetter(i));
    }
  }, cb);
}


EndPoint.prototype.getLowerBounds = function(pairs, cb) {
  withTransaction(this.db, function(T, cb) {
    getPerPairData(T, pairs, getLowerBound, 'lb', cb);
  }, cb);
}

EndPoint.prototype.updateLowerBounds = function(pairs, cb) {
  var self = this;
  Q.all(pairs.map(function(pair) {
    return Q.ninvoke(self, "updateLowerBound", pair.src, pair.dst, pair.lb);
  })).then(function(lbs) {
    cb(null, lbs);
  }).fail(function(err) {
    cb(err);
  });
}

EndPoint.prototype.getUpperBounds = function(pairs, cb) {
  withTransaction(this.db, function(T, cb) {
    getPerPairData(T, pairs, getUpperBound, 'ub', cb);
  }, cb);
}


var packetHandlerParamNames = ['endPoint', 'packet'];
EndPoint.prototype.addPacketHandler = function(handler) {
  var paramNames = common.getParamNames(handler);
  assert(paramNames.length == packetHandlerParamNames.length);
  for (var i = 0; i < paramNames.length; i++) {
    if (paramNames[i] != packetHandlerParamNames[i]) {
      throw new Error('Expected names ' + packetHandlerParamNames + ' but got ' + paramNames);
    }
  }
  this.packetHandlers.push(handler);
}

EndPoint.prototype.setIsLeaf = function(x) {
  this.isLeaf = x;
}

EndPoint.prototype.callPacketHandlers = function(p) {
  for (var i = 0; i < this.packetHandlers.length; i++) {
    this.packetHandlers[i](this, p);
  }
}

EndPoint.prototype.putPacket = function(packet, cb) {
  var self = this;
  withTransaction(this.db, function(T, cb) {
    getLowerBound(T, packet.src, packet.dst, function(err, lb) {
      if (err) {
        cb(err);
      } else if (packet.seqNumber < lb) {
        cb();
      } else {
        if (self.name == packet.dst) {
          try {
            self.callPacketHandlers(packet);
            setLowerBound(T, packet.src, packet.dst, bigint.inc(packet.seqNumber), cb);
          } catch (e) {
            cb(e);
          }
        } else {
          getPacket(T, packet.src, packet.dst, packet.seqNumber, function(err, packet2) {
            if (err) {
              cb(err);
            } else {
              if (packet2) {
                if (eq(packet, packet2)) {
                  cb();
                } else {
                  cb(new Error('A different packet has already been delivered'));
                }
              } else {
                T.run(
                  'INSERT INTO packets VALUES (?, ?, ?, ?, ?)',
                  packet.src, packet.dst, packet.seqNumber, packet.label, packet.data, cb);
              }
            }
          });
        }
      }
    });
  }, cb);
}

function getAllFromTable(db, tableName, cb) {
  db.all('SELECT * FROM ' + tableName + ';', cb);
}

EndPoint.prototype.disp = function(cb) {
  var self = this;
  getAllFromTable(self.db, 'packets', function(err, packets) {
    if (err) {
      cb(err);
    } else {
      getAllFromTable(self.db, 'lowerBounds', function(err, lowerBounds) {
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

EndPoint.prototype.close = function(cb) {
  this.db.close(cb);
  this.db = null;
}

EndPoint.prototype.open = function(cb) {
  var self = this;
  if (!self.db) {
    openDBWithFilename(this.dbFilename, function(err, db) {
      self.db = db;
      cb(err);
    });
  } else {
    cb();
  }
}

EndPoint.prototype.makeVerbose = function() {
  schema.makeVerbose(this);
}

function tryMakeEndPointFromFilename(dbFilename, cb) {
  var mailboxName = naming.getMailboxNameFromFilename(dbFilename);
  if (mailboxName) {
    tryMakeEndPoint(dbFilename, mailboxName, cb);
  } else {
    cb(new Error('Unable to extract mailboxname from filename ' + dbFilename));
  }
}

function isEndPoint(x) {
  if (typeof x == 'object') {
    return x.constructor.name == 'EndPoint';
  }
  return false;
}

function withEP(ep, epOperation, done) {
  ep.open(function(err) {
    if (err) {
      console.log('Failed to open mailbox');
      done(err);
    } else {
      console.log('Lets run operation');
      epOperation(ep, function(err) {
        ep.close(function(err2) {
          done(err || err2);
        });
      });
    }
  });
}


module.exports.EndPoint = EndPoint;
module.exports.isEndPoint = isEndPoint;
module.exports.tryMakeEndPoint = tryMakeEndPoint;
module.exports.tryMakeAndResetEndPoint = tryMakeAndResetEndPoint;
module.exports.srcDstPairUnion = srcDstPairUnion;
module.exports.srcDstPairIntersection = srcDstPairIntersection;
module.exports.srcDstPairDifference = srcDstPairDifference;
module.exports.filterByName = filterByName;
module.exports.tryMakeEndPointFromFilename = tryMakeEndPointFromFilename;
module.exports.withEP = withEP;
