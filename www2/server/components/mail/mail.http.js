// A remote mailbox that we can play with, over HTTP.

var ServerConnection = require('./server-connection.js');
var coder = require('./json-coder.js');
var schema = require('./mailbox-schema.js');
var assert = require('assert');


function toJson(method, data) {
    return (method.httpMethod == 'get'? JSON.parse(data) : data);
}


// Make a method to put in the local mailbox object
// that will result in an HTTP request according
// to the schema.
function makeMethod(scon, mailboxName, method) {
    return function() {
	var allArgs = Array.prototype.slice.call(arguments);
	var lastArgIndex = allArgs.length - 1;
	var args = allArgs.slice(0, lastArgIndex);
	var cb = allArgs[lastArgIndex];

	var responseHandler = function(err, body) {
	    if (err) {
		cb(toJson(err));
	    } else {
		var output = method.output;
		var data = coder.decodeArgs(
		    output,

		    // Don't know why we get the response as a string when
		    // sending a get request. Probably related to the
		    // 'request library'.
		    toJson(method, body)
		);
		
		if (data == undefined) {
		    cb(new Error('Failed to decode HTTP response'));
		} else {
		    cb.apply(null, data);
		}
	    }
	};

	if (method.httpMethod == 'post') {
	    scon.makePostRequest(
		mailboxName,
		method.name,
		coder.encodeArgs(method.input, args),
		responseHandler
	    );
	} else {
	    assert(args.length == method.input.length);
	    scon.makeGetRequest(
		mailboxName,
		method.name,
		coder.encodeGetArgs(method.input, args),
		responseHandler
	    );
	}
    }
}

function Mailbox(serverConnection, mailboxName) {
    this.mailboxName = mailboxName;
    for (methodName in schema.methods) {
	this[methodName] = makeMethod(
	    serverConnection,
	    mailboxName,
	    schema.methods[methodName]
	);
    }
}

// Call this function when you need a new mailbox.
function tryMakeMailbox(serverAddress, userdata, mailboxName, cb) {
    var s = new ServerConnection(serverAddress);
    s.login(userdata, function(err) {
	if (err) {
	    cb(err);
	} else {
	    // Register these as rpc calls.
	    cb(undefined, new Mailbox(s, mailboxName));
	}
    });
}

module.exports.tryMakeMailbox = tryMakeMailbox;
