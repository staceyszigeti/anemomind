var files = require('../files.js');
var fs = require('fs');
var assert = require('assert');
var Q = require('q');
var mail2 = require('../mail2.sqlite.js');
var sync2 = require('../sync2.js');
var common = require('../common.js');
var epschema = require('../endpoint-schema.js');

function xfun(cb) {
  setTimeout(function() {cb(null, 119);}, 130);
}

function yfun(cb) {
  setTimeout(function() {cb(null, 8889);}, 140);
}


function r(cb) {
  console.log('Calling r');
  cb(null, 999);
}


describe('files', function() {
  it('packfiles', function(done) {
    fs.writeFile('/tmp/filestest.txt', 'Some file data', function(err) {
      assert(!err);
      fs.readFile('/tmp/filestest.txt', function(err, refdata) {
        assert(!err);
        files.packFiles([{src:'/tmp/filestest.txt', dst:'mjao.txt'}]).then(function(values) {
          var value = values[0];
          assert(value.src.equals(refdata));
          assert.equal(value.dst, 'mjao.txt');
          var root = '/tmp/rulle/abc';
          files.unpackFiles(root, values).then(function(filenames) {
            assert.equal(filenames[0], '/tmp/rulle/abc/mjao.txt');
            fs.readFile('/tmp/rulle/abc/mjao.txt', function(err, unpackedData) {
              assert(!err);
              assert(unpackedData.equals(refdata));
              done();
            });
          });
        });
      });
    });
  });

  it('promises', function(done) {
    var X = Q.nfcall(xfun);
    var Y = Q.nfcall(yfun);

    X.then(Y).then(function(y) {
      assert(y == 119); // What X resolves to... why?
      done();
    });
  });

  it('promises', function(done) {
    var X = Q.nfcall(xfun);
    var Y = Q.nfcall(yfun);

    X.then(function() {return Y;}).then(function(y) {
      assert(y == 8889);
      done();
    });
  });

  it('transferfiles', function(done) {
    Q.all([
      Q.nfcall(mail2.tryMakeAndResetEndPoint, '/tmp/epa.db', 'a'),
      Q.nfcall(mail2.tryMakeAndResetEndPoint, '/tmp/epb.db', 'b')
      ]).then(function(eps) {
        var a = eps[0];
        var b = eps[1];
        b.addPacketHandler(
          files.makePacketHandler('/tmp/boxdata', true, function(err) {
            Q.all([
              Q.nfcall(fs.readFile, '/tmp/boxdata/boat.dat'),
              Q.nfcall(fs.readFile, '/tmp/boat.dat')
            ]).then(function(fdata) {
              assert(fdata[0].equals(fdata[1]));
              done();
            });
          }));
        epschema.makeVerbose(a);
        epschema.makeVerbose(b);
        
        var srcFilename = '/tmp/boat.dat';
        Q.nfcall(fs.writeFile, srcFilename, 'Interesting data')
          .then(common.pfwrap(9))
          .then(function() {
            return files.sendFiles(a, 'b', [{src: srcFilename, dst: 'boat.dat'}]);
          })
          .then(function() {
            return Q.nfcall(sync2.synchronize, a, b);
          });
      });
  });
});
