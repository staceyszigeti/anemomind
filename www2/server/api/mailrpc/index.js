'use strict';

var express = require('express');
var router = express.Router();
var rpc = require('./rpc.js');
var auth = require('../../auth/auth.service');


/*

  Available functions to call, bound to this router.
  This list can be generated using rpc.makeOverview():

  ========= Essential calls ========
  Function name: putPacket
  HTTP-call: POST /putPacket/:mailboxName

  Function name: getPacket
  HTTP-call: GET /getPacket/:mailboxName/:src/:dst/:seqNumber

  Function name: getSrcDstPairs
  HTTP-call: GET /getSrcDstPairs/:mailboxName

  Function name: setLowerBound
  HTTP-call: GET /setLowerBound/:mailboxName/:src/:dst/:lowerBound

  Function name: getLowerBounds
  HTTP-call: POST /getLowerBounds/:mailboxName

  Function name: getUpperBounds
  HTTP-call: POST /getUpperBounds/:mailboxName

  ========= To facilitate unit testing ========
  Function name: getTotalPacketCount
  HTTP-call: GET /getTotalPacketCount/:mailboxName

  Function name: sendPacket
  HTTP-call: POST /sendPacket/:mailboxName

  Function name: reset
  HTTP-call: GET /reset/:mailboxName
  
  All these functions will return JSON data in the body. On success, the
  HTTP status code is 200 and the body data is the result.
  On failure, the status code is 500 and the body data
  is the error.
  
*/

rpc.bindMethodHandlers(router, auth.isAuthenticated());
module.exports = router;
