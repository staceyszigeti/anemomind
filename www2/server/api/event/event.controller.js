'use strict';

var Q = require('q');
var _ = require('lodash');
var Event = require('./event.model');
var mongoose = require('mongoose');
var multer  = require('multer');
var fs = require('fs');
var config = require('../../config/environment');
var mkdirp = require('mkdirp');

var boatAccess = require('../boat/access.js');
var backup = require('../../components/backup');

var tiles = require('../tiles/tiles.controller.js');

// Encoding hex mongoid in urls is too long.
// we should switch to https://www.npmjs.com/package/hashids
// at some point.
// In the mean time, this function can still be used.
function stringIsObjectId(a) {
  return /^[0-9a-fA-F]{24}$/.test(a);
}

var canRead = function(req, event) {
  // A user can access a note if he/she is the author
  if (req.user && req.user.id && event.author == req.user.id) {
    return Q.Promise(function(resolve, reject) {
      resolve(req.user.id, event);
    });
  }

  // Otherwise, the user needs read access to the boat the note is
  // attached to.
  return boatAccess.userCanReadBoatId((req.user ? req.user.id : undefined),
                                      event.boat);
}

var canWrite = function(req, event) {
  return boatAccess.userCanWriteBoatId(req.user.id, event.boat);
}

function sendEventsWithQuery(res, query) {
  Event.find(query, function (err, events) {
    if(err) { return handleError(res, err); }

    var promises = [];
    for (var i in events) {
      promises.push(extendEventWithBoatData(events[i]));
    }
     
    Q.all(promises).then(function(_events) {
      console.warn('EVENTS: ====> ');
     console.warn(_events);
      res.status(200).json(_events);
    })
    .catch(function (err) {
      console.log(err + (err.stack ? err.stack : ''));
      res.status(500);
    });
  });
}

// Get the latest readable events
//
// Because there might be a large number of public events,
// listing them all would be too much.
// Only events attached to boats associated with a user are listed
// We should add a way for users to "subscribe" to public boats.
exports.index = function(req, res) {
  try {
    var query = { };

    var handleDateParam = function(param, operator) {
      if (req.query[param] && req.query[param] != "") {
        var date = new Date(req.query[param]);
        if (isNaN(date)) {
          return res.sentStatus(400);
        }
        if (!query.when) {
          query.when = { };
        }
        query.when[operator] = date;
      }
    }

    handleDateParam('B', '$lte');
    handleDateParam('A', '$gte');

    if (req.query.b) {
      boatAccess.userCanReadBoatId((req.user ? req.user.id : undefined),
                                   req.query.b)
      .then(function() {
         query.boat = req.query.b;
         sendEventsWithQuery(res, query);
      })
      .catch(function(err) { res.sendStatus(403); });
    } else {
      boatAccess.readableBoats(req)
        .then(function(boats) {
          if (boats.length == 0) {
            return res.status(200).json([]);
          }
          query.boat = { $in : _.map(boats, '_id') };
          sendEventsWithQuery(res, query);
        })
        .catch(function(err) { res.sendStatus(403); });
    }
  } catch(err) {
    console.warn(err);
    console.warn(err.stack);
    res.sendStatus(500);
  }
};

function extendEventWithBoatData(ev) {
  var deferred = Q.defer();
  ev.dataAtEventTime = undefined;
  tiles.boatInfoAtTime(ev.boat, ev.when, function(err, info) {
    if (err) {
      deferred.reject(err);
    } else {
      deferred.resolve(info);
    }
  });


  return deferred.promise.then(function(info) {
      var result = _.clone(ev._doc); 
      result.dataAtEventTime = info;
      return result;
    });
};

// Get a single event
exports.show = function(req, res) {
  Event.findById(req.params.id, function (err, event) {
    if(err) { return handleError(res, err); }
    if(!event) { return res.sendStatus(404); }

    canRead(req, event)
    .then(function() {
      return extendEventWithBoatData(event);
    })
    .then(function(event) {
      res.json(event);
    })
    .catch(function(err) {
      console.log(err + (err.stack ? err.stack : ''));
      res.sendStatus(403);
    });
  });
};

// Creates a new event in the DB.
exports.create = function(req, res) {
  try {
  if (!req.user) {
    return res.sendStatus(401);
  }
  if (!req.body.boat) {
    return res.sendStatus(400);
  }

  if (!stringIsObjectId(req.body.boat)) {
    return res.sendStatus(400);
  }

  var user = mongoose.Types.ObjectId(req.user.id);
  var event = req.body;
  event.author = user;
  event.boat = mongoose.Types.ObjectId(req.body.boat);

  canWrite(req, event)
    .then(function() {
      // User is allowed to add a note for this boat.

      Event.create(event, function(err, event) {
                   if(err) { return handleError(res, err); }
                   return res.status(201).json(event);
                   });
    })
    .catch(function(err) {
      res.sendStatus(403);
    });
  } catch(err) {
    console.warn(err);
    console.warn(err.stack);
    res.sendStatus(500);
  }
};

exports.remove = function(req, res) {
  if (!req.user) {
    return res.sendStatus(401);
  }
  if (!req.body.boat) {
    return res.sendStatus(400);
  }
  boatAccess.userCanWriteBoatId(req.user.id, req.body.boat)
    .then(function() {
      // User is allowed to add a note for this boat.
      var user = mongoose.Types.ObjectId(req.user.id);
      var event = req.body;
      event.author = user;
      event.boat = mongoose.Types.ObjectId(req.body.boat);

      Event.create(event, function(err, event) {
                   if(err) { return handleError(res, err); }
                   return res.status(201).json(event);
                   });
    })
    .catch(function(err) { res.sendStatus(403); });
};

function handleError(res, err) {
  console.log('error: ' + err);
  return res.sendStatus(500, err);
}


var photoUploadPath = fs.realpathSync(config.uploadDir) + '/photos/';
console.log('Uploading photos to: ' + photoUploadPath);

// mkdirp is async, so we take the opportunity to run before multer with
// express chained handlers
exports.createUploadDirForBoat = function(req, res, next) {
  if (req.params.boatId && req.params.boatId.match(/^[0-9a-zA-Z_-]+$/)) {
    mkdirp(photoUploadPath + '/' + req.params.boatId, next);
  } else {
    next();
  }
};

// This handler can assume that the user is authentified, it has write access,
// and the upload folder for the boat has been created.
exports.postPhoto = multer({
  dest: photoUploadPath,

  rename: function (fieldname, filename) {
    // Validation is done in onFileUploadStart
    return filename;
  },

  onFileUploadStart: function (file, req, res) {
    // Recognizes a UUID as generated by the app
    var uuidRegexp = 
      /^[A-F0-9]{8}-[A-F0-9]{4}-4[A-F0-9]{3}-[89aAbB][A-F0-9]{3}-[A-F0-9]{12}.jpg$/;

    if (file.originalname.match(uuidRegexp)) {
      if (!fs.existsSync(file.path)) {
        console.log('photoHandler: accepting file: ' + file.originalname);
        return true;
      } else {
        console.log('photoHandler: replacing existing photo: '
                    + file.originalname);
        return true;
      }
    }
    console.log('photoHandler: rejecting bad filename: '
                + file.originalname);
    res.sendStatus(400);
    return false;
  },

  changeDest: function(dest, req, res) {
    if (req.params.boatId && req.params.boatId.match(/^[0-9a-zA-Z_-]+$/)) {
      var dir = dest + '/' + req.params.boatId;
      return dir;
    }
    return dest;
  },

  onFileUploadComplete: function(file, req, res) {
    res.sendStatus(201);
    backup.backupPhotos();
  }
});

exports.photoUploadPath = photoUploadPath;
