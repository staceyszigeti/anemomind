'use strict';

var express = require('express');
var router = express.Router();
var rpc = require('./rpc.js');
var auth = require('../../auth/auth.service');


/*

  Available functions to call, bound to this router.
  This list can be generated using rpc.makeOverview():
  
  Function name: setForeignDiaryNumber
  HTTP-call: GET /setForeignDiaryNumber/:mailboxName/:otherMailbox/:newValue

  Function name: getFirstPacketStartingFrom
  HTTP-call: GET /getFirstPacketStartingFrom/:mailboxName/:diaryNumber/:lightWeight

  Function name: handleIncomingPacket
  HTTP-call: POST /handleIncomingPacket/:mailboxName

  Function name: isAdmissible
  HTTP-call: GET /isAdmissible/:mailboxName/:src/:dst/:seqNumber

  Function name: getForeignDiaryNumber
  HTTP-call: GET /getForeignDiaryNumber/:mailboxName/:otherMailbox

  Function name: getForeignStartNumber
  HTTP-call: GET /getForeignStartNumber/:mailboxName/:otherMailbox

  Function name: reset
  HTTP-call: GET /reset/:mailboxName

  Function name: sendPacket
  HTTP-call: POST /sendPacket/:mailboxName

  Function name: getTotalPacketCount
  HTTP-call: GET /getTotalPacketCount/:mailboxName

  
  All these functions will return JSON data in the body. On success, the
  HTTP status code is 200 and the body data is the result.
  On failure, the status code is 500 and the body data
  is the error.
  
*/

rpc.bindMethodHandlers(router, auth.isAuthenticated());
module.exports = router;
