var builder = require('../components/RpcMailbox.js');
var mb = require('../components/LocalMailbox.js');
var assert = require('assert');

var rpcTable = {};
builder.fillTable(rpcTable);

var mailboxName = 'rulle';

function prepareMailbox(cb) {
  rpcTable.mb_reset({
    name: mailboxName,
  }, function(response) {
    assert.equal(response.error, undefined);
    cb(response);
  });
}

describe(
  'reset',
  function() {
    mb.setMailRoot('/tmp/anemobox/');
    it(
      'Should reset the mailbox and make sure the packet count is 0',
      function(done) {
	prepareMailbox(function(response) {
	  rpcTable.mb_getTotalPacketCount({
	    name: mailboxName
	  }, function(response) {
	    assert.equal(response.error, undefined);
	    assert.equal(response.result, 0);
	    done();
	  });
	});
      }
    );
  }
);

describe(
  'getForeignDiaryNumber',
  function() {
    it(
      'Should get a foreign diary number from an empty mailbox',
      function(done) {
	mb.setMailRoot('/tmp/anemobox/');
	prepareMailbox(function(response) {
	  assert.equal(response.error, undefined);
	  rpcTable.mb_getLowerBound({
	    name: mailboxName,
	    src: 'a',
            dst: 'b',
	  }, function(response) {
	    assert.equal(response.result, "0000000000000000");
	    rpcTable.mb_setLowerBound({
	      name: mailboxName,
              src: 'a',
              dst: 'b',
	      lowerBound: "0000000000000009"
	    }, function(response) {
	      assert.equal(response.error, undefined);
	      rpcTable.mb_getLowerBound({
		name: mailboxName,
                src: 'a',
                dst: 'b',
	      }, function(response) {
		assert.equal(response.result, "0000000000000009");
		done();
	      });
	    });
	  });
	});
      }
    );
  }
);

describe(
  'error',
  function() {
    it(
      'Should result in an error',
      function(done) {
	mb.setMailRoot('/tmp/anemobox/');
	rpcTable.mb_reset({
	  // Omit 'name'
	}, function(response) {
	  assert(response.error);
	  done();
	})
      }
    );
  }
);

describe(
  'error2',
  function() {
    it(
      'Should result in an error',
      function(done) {
	mb.setMailRoot('/tmp/anemobox/');
	rpcTable.mb_getLowerBound({
	  name: mailboxName
	  // omit the required argument for the function
	}, function(response) {
          console.log(response);
	  assert(response.error);
	  done();
	})
      }
    );
  }
);
