// Emulate candump -L can0
var j1939socket = require('j1939socket');
var can = require('socketcan');
var buffer = require('buffer');
var anemonode = require('../build/Release/anemonode');
var logger = require('./logger.js');

var verbose = false;
var counter = 0;

var running = false;

function logRawPacket(msg) {

  // At the end of this function,
  // we will trigger a fetch of j1939, even
  // if we are not logging the raw NMEA 2000 data.

  if (running) {
    var systemTime0 = 1000*msg.ts_sec + 0.001*msg.ts_usec;
    var systemTime1 = new Date().getTime();
    var monotonicTime1 = anemonode.currentTime();
    
    // Solve this EQ: 
    // monotonicTime1 - monotonicTime0 = systemTime1 - systemTime0
    monotonicTime0 = monotonicTime1 - (systemTime1 - systemTime0);

    if (logger.logRawNmea2000(
      monotonicTime0,
      msg.id, msg.data)) {
      if (verbose && counter % 10 == 0) {
        console.log("Raw NMEA 2000 packet %d logged, time=%j id=%j data=%j", 
      	            counter, monotonicTime0, msg.id, msg.data);
      }
      counter++;
    }
  }
  
  // This will, hopefully, trigger a packet to be
  // delivered, inside "nmea2000.js".
  j1939socket.fetch();
}

var channel = null;

function startChannelIfNotAlready() {
  if (!channel) {
    try {
      channel = can.createRawChannel("can0", true /* ask for timestamps */);
      channel.start();
      channel.addListener("onMessage", logRawPacket);
    } catch (e) {
      console.log("Failed to listen to CAN channel");
      console.log(e);
      channel = null;
    }
  }
}

function start() { 
  running = !!channel;
}

function stop() {
  running = false;
}

function setState(r) {
  if (r) {
    start();
  } else {
    stop();
  }
}

// Always start it
startChannelIfNotAlready();

module.exports.start = start;
module.exports.stop = stop
module.exports.setState = setState;
