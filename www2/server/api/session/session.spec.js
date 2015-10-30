'use strict'

var should = require('should');
var app = require('../../app');
var request = require('supertest');
var Session = require('./session.model.js');
var User = require('../user/user.model.js');
var Boat = require('../boat/boat.model.js');
var mongoose = require('mongoose');
var Schema = mongoose.Schema;

var boatId = Schema.ObjectId(119);

function prepareUser(cb) {
  User.remove().exec().then(function() {
    var testUser = new User({
      "provider" : "local",
      "name" : "test",
      "email" : "test@anemomind.com",
      "hashedPassword" : "bj0zHvlC/YIzEFOU7nKwr+OHEzSzfdFA9PMmsPGnWITGHp1zlL+29oa049o6FvuR2ofd8wOx2nBc5e2n2FIIsg==",
      "salt" : "bGwuseqg/L/do6vLH2sPVA==",
      "role" : "user"
    });
    testUser.save(cb);
  });
}

function getFirstUser(cb) {
  User.findOne({}, cb);
}

function getFirstBoat(cb) {
  Boat.findOne({}, cb);
}

function prepareBoat(cb) {
  getFirstUser(function(err, user) {
    Boat.remove().exec().then(function() {
      var testBoat = new Boat({
        name: "Frida",
        type: "IF",
        sailNumber: "1604",
        length: "8",
        lengthUnit: "meter",
        admins: [user._id],
        readers: [user._id]
      });
      testBoat.save(cb);
    });
  });
}

function prepareUserAndBoat(cb) {
  prepareUser(function(err) {
    if (err) {
      cb(err);
    } else {
      prepareBoat(cb);
    }
  });
}

function prepareRecord(cb) {
  prepareUserAndBoat(function(err) {
    if (err) {
      cb(err);
    } else {
      Session.remove({}, function(err) {
        if (err) {
          cb(err);
        } else {
          getFirstBoat(function(err, boat) {
            var boatId = boat._id;
            Session.create({
              boat: boatId,
              _id: "s123",
              maxSpeedOverGround: 7.8,
              trajectoryLength: 8.9
            }, function(err, sessionId) {
              cb(err, sessionId, boatId);
            });
          });
        }
      });
    }
  });
}

describe('Session', function() {
  it('GET /api/session', function(done) {
    prepareRecord(function(err, id, boatId) {
      if (err) {
        done(err);
      } else {
        request(app)
          .get('/api/session/s123')
          .expect(200)
          .end(function(err, res) {
            if (err) {
              done(err);
            } else {
              res.body.should.have.property('maxSpeedOverGround');
              res.body.maxSpeedOverGround.should.equal(7.8);
              done();
            }
          });
      }
    });
  });
  
  it('GET /api/session/boat', function(done) {
    prepareRecord(function(err, id, boatId) {
      if (err) {
        done(err);
      } else {
        for (var i = 0; i < 30; i++) {
          console.log("The boatId is " + boatId);
        }
        Session.find({}, function(err, data) {
          console.log('DATA');
          console.log(data);
        });
        var addr = '/api/session/boat/' + boatId;
        request(app)
          .get(addr)
          .expect(200)
          .end(function(err, res) {
            if (err) {
              done(err);
            } else {
              var body = res.body;
              console.log('BODY:');
              console.log(body);
              res.body.should.be.instanceof(Array);
              var session = res.body[0];
              session.should.have.property('maxSpeedOverGround');
              session.maxSpeedOverGround.should.equal(7.8);
              done();
            }
          });
      }
    });    
  });
  
});
