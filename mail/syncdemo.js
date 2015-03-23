// A simulation of mailbox synchronizations
var mb = require("./mail.sqlite.js");
var assert = require('assert');
var async = require("async");
var pkt = require("./packet.js");
var q = require("q");

var boxnames = ["A", "B", "C"];


function fillWithPackets(count, srcMailbox, dstMailboxName, cb) {
    assert(typeof count == 'number');
    assert(typeof dstMailboxName == 'string');
    if (count == 0) {
	cb();
    } else {
	srcMailbox.sendPacket(
	    dstMailboxName,
	    "Some-label" + count,
	    new Buffer(3),
	    function (err) {
		if (err == undefined) {
		    fillWithPackets(count-1, srcMailbox, dstMailboxName, cb)
		} else {
		    cb(err);
		}
	    }
	);
    }
}



function finishSync(index, boxA, boxB, cb) {
    // Where are done synchronizing. Save the index and
    // move on.
    boxA.setForeignDiaryNumber(
	boxB.mailboxName,
	index,
	cb
    );
}

function fetchFullPacket(index, boxA, boxB, cb) {
    boxB.getFirstPacketStartingFrom(
	index,
	false,
	function (err, row) {
	    if (err == undefined) {

		if (row == undefined) {
		    console.log('WARNING: We wouldnt expect the packet to be empty');
		    finishSync(index, boxA, boxB, cb);
		} else {

		    // Create a packet object
		    var packet = new pkt.Packet(
			row.src,
			row.dst,
			row.seqnumber,
			row.cnumber,
			row.label,
			row.data
		    );

		    // The actual index of the packet
		    // that we fetched.
		    var newIndex = row.diarynumber;

		    
		    boxA.handleIncomingPacket(
			packet,
			function (err) {
			    
			    if (err == undefined) {
				
				// Recur with the next index.
				synchronizeDirectedFrom(
				    // IMPORTANT: Call with the index of the
				    // packet fetched here.
				    newIndex + 1,
				    boxA, boxB, cb
				);
			    } else {
				cb(err);
			    }
			}
		    );
		}
		
	    } else {
		cb(err);
	    }
	}
    );
}

function handleSyncPacketLight(index, lightPacket, boxA, boxB, cb) {
    if (lightPacket == undefined) {

	finishSync(index, boxA, boxB, cb);
	
    } else { 
	boxA.isAdmissible(
	    lightPacket.src, lightPacket.dst, lightPacket.seqnumber,
	    function(err, admissible) {
		if (err == undefined) {
		    if (admissible) {

			// It is admissible. Let's fetch the full packet.
			fetchFullPacket(
			    index,
			    boxA,
			    boxB,
			    cb
			);
			
		    } else {
			// Recur, with next index.
			synchronizeDirectedFrom(index+1, boxA, boxB, cb);
		    }
		} else {
		    cb(err);
		}
	    }
	);
    }
}

function synchronizeDirectedFrom(startFrom, boxA, boxB, cb) {
    // Retrieve a light-weight packet
    // just to see if we should accept it
    boxB.getFirstPacketStartingFrom(
	startFrom,
	true,
	function(err, row) {
	    if (err == undefined) {
		handleSyncPacketLight(startFrom, row, boxA, boxB, cb);
	    } else {
		cb(err);
	    }	    
	}
    );
}

// Synchronize state in only one direction,
// so that boxA will know everything that boxB knows,
// but not the other way around.
function synchronizeDirected(boxA, boxB, cb) {
    // First retrieve the first number we should ask for
    boxA.getForeignStartNumber(
	boxB.mailboxName,
	function(err, startFrom) {
	    if (err == undefined) {
		synchronizeDirectedFrom(startFrom, boxA, boxB, cb);
	    } else {
		cb(err);
	    }
	}
    );
}


// Perform a full synchronization of the contents in the two mailboxes.
// boxB will share its packets with boxA, and boxA will share its packets with boxB.
function synchronize(boxA, boxB, cb) {
    synchronizeDirected(
	boxA, boxB,
	function(err) {
	    synchronizeDirected(
	    	boxB, boxA,
	    	cb
	    );
	}
    );
}

// Perform pairwise synchronization of mailboxes, from left to right.
function synchronizeArray(mailboxes, cb) {
    if (mailboxes.length < 2) {
	cb();
    } else {
	synchronize(
	    mailboxes[0],
	    mailboxes[1],
	    function (err) {
		if (err == undefined) {
		    synchronizeArray(mailboxes.slice(1), cb);
		} else {
		    cb(err);
		}
	    }
	);
    }
}

function synchronizeForthAndBack(mailboxes, cb) {
    synchronizeArray(
	mailboxes,
	function (err) {
	    synchronizeArray(
		mailboxes.reverse(),
		cb
	    );
	}
    );
}



function dispMailboxes(mailboxes, cb) {
    if (mailboxes.length == 0) {
	cb();
    } else {
	var box = mailboxes[0];
	console.log('\n\n MAILBOX NAMED ' + box.mailboxName);
	mb.dispAllTableData(
	    box.db,
	    function (err) {
		dispMailboxes(mailboxes.slice(1), cb);
	    }
	);		    
    }
}

function someSpace(s) {
    for (var i = 0; i < 9; i++) {
	console.log(s);
    }
}
    

function startSync(err, mailboxes) {
    if (err == undefined) {
	synchronizeForthAndBack(
	    mailboxes,
	    function (err) {
		someSpace('---------------------- DONE SYNC --------------------------------');
		dispMailboxes(
		    mailboxes, function(err) {
			console.log('Done synchronizing');
			assert(err == undefined);
		    }
		);
	    }
	);
    } else {
	console.log('Something failed: %j', err);
    }
}


// Called once the first mailbox has been filled
function mailboxesCreated(err, mailboxes) {

    someSpace('');

    var PACKETCOUNT = 39;
    
    fillWithPackets(
	PACKETCOUNT,
	mailboxes[0],
	mailboxes[2].mailboxName,
	function(err) {
	    startSync(err, mailboxes);
	}
    );
}



// Main call to run the demo
async.map(
    boxnames,
    function (boxname, cb) {
	var box = new mb.Mailbox(":memory:", boxname, function(err) {
	    cb(err, box);
	});
    },
    mailboxesCreated
);
