var builder = require('../components/RpcMailbox.js');
var assert = require('assert');

var rpcTable = {};
builder.fillTable(rpcTable);

var mailboxName = 'rulle';

function prepareMailbox(cb) {
    rpcTable.mb_reset({
	thisMailboxName: mailboxName,
    }, function(response) {
	assert(response.error == undefined);
	cb(response);
    });
}

describe(
    'reset',
    function() {
	it(
	    'Should reset the mailbox and make sure the packet count is 0',
	    function(done) {
		prepareMailbox(function(response) {
		    rpcTable.mb_getTotalPacketCount({
			thisMailboxName: mailboxName
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
		prepareMailbox(function(response) {
		    assert.equal(response.error, undefined);
		    rpcTable.mb_getForeignDiaryNumber({
			thisMailboxName: mailboxName,
			otherMailbox: "evian"
		    }, function(response) {
			assert.equal(response.result, undefined);
			rpcTable.mb_setForeignDiaryNumber({
			    thisMailboxName: mailboxName,
			    otherMailbox: "evian",
			    newValue: "0000000000000009"
			}, function(response) {
			    assert.equal(response.error, undefined);
			    rpcTable.mb_getForeignDiaryNumber({
				thisMailboxName: mailboxName,
				otherMailbox: "evian"
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
		rpcTable.mb_reset({
		    // Omit 'thisMailboxName'
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
		rpcTable.mb_getForeignDiaryNumber({
		    thisMailboxName: mailboxName
		    // omit the required argument for the function
		}, function(response) {
		    assert(response.error);
		    done();
		})
	    }
	);
    }
);
