var util = require('util');
var bleno = require('bleno');
var anemonode = require('../build/Release/anemonode');

var BlenoPrimaryService = bleno.PrimaryService;
var BlenoCharacteristic = bleno.Characteristic;

function pad (str, max) {
  return str.length < max ? pad("0" + str, max) : str;
}

var queue = [];
var queuedEntries = {};
var sending = false;

function next() {
  if (sending) {
    return;
  }
  if (queue.length == 0) {
    return;
  }
  var d = queue.shift();
  sending = true;
  d.updateValueCallback(encodeValueInBuffer(d.entry));
  delete queuedEntries[d.entry];
};


//Define a class to adapt dispacher entries to characteristics.
function DispatcherCharacteristic(entry) {
  this.entry = entry;
  DispatcherCharacteristic.super_.call(this, {
    uuid: 'AFF1E42D-EF91-456F-86FA-8703'
          + pad(anemonode.dispatcher.values[entry].dataCode.toString(16), 8),
    properties: ['notify', 'read']
  });
}
util.inherits(DispatcherCharacteristic, BlenoCharacteristic);

function encodeValueInBuffer(entry) {
  var dispatchData = anemonode.dispatcher.values[entry];

  function formatFloat(value) {
    var buffer = new Buffer(4);
    buffer.writeFloatLE(value, 0, 4);
    return buffer;
  }
  function format64bits(value) {
    // is it LE or BE?
    var buffer = new Buffer(8);
    buffer.writeIntLE(value, 0, 8);
    return buffer;
  }
  function formatPos(value) {
    // is it LE or BE?
    var buffer = new Buffer(2* 8);
    buffer.writeDoubleLE(value.lon, 0, 8);
    buffer.writeDoubleLE(value.lat, 8, 8);
    return buffer;
  }
  var formatValue = {
    "degrees": formatFloat,
    "knots": formatFloat,
    "nautical miles": formatFloat,
    "WGS84 latitude and longitude, in degrees": formatPos,
    "seconds since 1.1.1970, UTC": format64bits
  };
  
  var unit = dispatchData.unit;
  if (!unit in formatValue) {
    // OK to crash, because all possible units are known at compile time.
    throw('Error: do not know how to format: ' + unit);
  }

  var value = dispatchData.value();
  return formatValue[unit](value);
}

DispatcherCharacteristic.prototype.onSubscribe = function(maxValueSize, updateValueCallback) {
  console.log(this.entry +' subscribe');
  var entry = this.entry;
  this.subscribeIndex = anemonode.dispatcher.values[this.entry].subscribe(
    function(val) {
      if (!(entry in queuedEntries)) {
        queue.push({updateValueCallback: updateValueCallback, entry: entry });
        queuedEntries[entry] = true;
        next();
      }
    }
  );
};

DispatcherCharacteristic.prototype.onUnsubscribe = function() {
  console.log(this.entry +' unsubscribe');
 
  if (this.subscribeIndex) {
    anemonode.dispatcher.values[this.entry].unsubscribe(this.subscribeIndex);
    delete this.subscribeIndex;
  }
};

DispatcherCharacteristic.prototype.onNotify = function() {
  // Called when the phone has been notified of a value change.
  sending = false;
  next();
};

DispatcherCharacteristic.prototype.onReadRequest = function(offset, callback) {
  if (anemonode.dispatcher.values[this.entry].length() == 0) {
    callback(this.RESULT_UNLIKELY_ERROR);
  } else {
    callback(this.RESULT_SUCCESS, encodeValueInBuffer(this.entry));
  }
};

module.exports.pushCharacteristics = function(array) {
  // Instanciate one DispatcherCharacteristic for each dispatcher entry
  for (var entry in anemonode.dispatcher.values) {
    array.push(new DispatcherCharacteristic(entry));
  }
}
