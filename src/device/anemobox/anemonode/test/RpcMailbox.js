var builder = require('../components/RpcMailbox.js');
var mb = require('../components/LocalMailbox.js');
var assert = require('assert');

var rpcTable = {};
builder.fillTable(rpcTable);

var mailboxName = null;

function prepareMailbox(cb) {
  mb.getName(function(lname) {
    mailboxName = lname;
    rpcTable.ep_reset({
      name: lname,
    }, function(response) {
      assert.equal(response.error, undefined);
      cb(response);
    });
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
	  rpcTable.ep_getTotalPacketCount({
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
  'updateLowerBounds',
  function() {
    it(
      'Should get the src dst pairs',
      function(done) {
	mb.setMailRoot('/tmp/anemobox/');
	prepareMailbox(function(response) {
	  assert.equal(response.error, undefined);
          assert(typeof mailboxName == 'string');
	  rpcTable.ep_updateLowerBounds({
	    name: mailboxName,
	    pairs: [{src:'a', dst:'b'}],
	  }, function(response) {
	    assert.equal(response.result[0], "0000000000000000");
	    rpcTable.ep_updateLowerBounds({
	      name: mailboxName,
              pairs: [{src: 'a',
                       dst: 'b',
	               lb: "0000000000000009"}]
	    }, function(response) {
	      assert.equal(response.error, undefined);
	      assert.equal(response.result[0], "0000000000000009");
	      done();
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
	rpcTable.ep_reset({
	  // Omit 'name' to provoke an error.
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

        prepareMailbox(function(response) {
	  rpcTable.ep_updateLowerBounds({
	    name: mailboxName
	    // omit the required argument for the function
	  }, function(response) {
	    assert(response.error);
	    done();
	  })
        });
      }
    );
  }
);
