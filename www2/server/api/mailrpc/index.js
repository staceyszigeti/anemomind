'use strict';

var express = require('express');
var router = express.Router();
var rpc = require('./rpc.js');
var auth = require('../../auth/auth.service');
var schema = require('../../components/mail/mailbox-schema.js');

var authenticator = auth.isAuthenticated();

// Register a GET or POST handler
// for every remote function that
// we can call.
for (var methodName in schema.methods) {
    rpc.bindMethodHandler(router, authenticator, schema.methods[methodName]);
}

module.exports = router;
