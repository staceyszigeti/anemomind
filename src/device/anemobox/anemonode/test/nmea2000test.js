var canutils = require('../components/canutils.js');
var assert = require('assert');

var exampleMessage = {"ts_sec":1498141174,
	"ts_usec":139588,"id":167576077,"ext":true,
	"data": new Buffer([42,159,2,29,20,250,255,255])};  
	
var ser = canutils.serializeMessage(exampleMessage);
console.log("ERROR: %j", canutils.getError(ser));
assert(!canutils.getError(ser))
var deser = canutils.deserializeMessage(ser);

assert(deser.id == exampleMessage.id);
assert(deser.ts_sec == exampleMessage.ts_sec);
assert(deser.ts_usec == exampleMessage.ts_usec);
assert(deser.data.equals(exampleMessage.data));

console.log("Serialized: %j", ser);
console.log("Deserialized: %j", deser);

