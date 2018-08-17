
var mongoose = require('mongoose');
var ChartTile = require('../chart/charttile.model');
var ChartSource = require('../chart/chartsource.model');
var expandArrays = require('../chart/expand-array').expandArrays;
var ObjectId = mongoose.Types.ObjectId;
var strftime = require('./strftime');

var dateLength = '2016-09-14T16:25:26'.length;





//// ESA log rendering

function renderEsaUndefined(data) {
  return "UNDEFINED";
}

/*

This is the code in AstraLoader.cpp.
Learn from it.

auto fracDeg = 0.01_deg;

LogFileHeaderSpec regataLogFileSpec{
  // Current, and things like that?
  {"Set", AstraValueParser()},
  {"Drift", AstraValueParser()},
  {"DRIFT", &ignoreAstraValue},
  {"SET", &ignoreAstraValue},

  // Wind
  {"WindType", AstraValueParser()},
  {"AWA", AstraValueParser()},
  {"AWS", AstraValueParser()},
  {"TWA", AstraValueParser()},
  {"TWS", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(TWS)))},// <-- See message2.txt
  {"TWD", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWD)))},// <-- See message2.txt
  {"GWS", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(GWS)))},// <-- See message2.txt
  {"GWD", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(GWD)))},// <-- See message2.txt

  // Not sure...
  {"FreezeWind", AstraValueParser()},
  {"FreezeTide", AstraValueParser()},

  // Time
  {"DateTimeTs", AstraValueParser()},
  {"Date", AstraValueParser(
      asDate("%d/%m/%y", FIELD_ACCESS(partialTimestamp)))},
  {"Time", AstraValueParser(asTimeOfDay(FIELD_ACCESS(timeOfDay)))},
  {"Ts", &ignoreAstraValue},

  // Meta
  {"DinghyID", AstraValueParser(asPrimitive<int>(FIELD_ACCESS(dinghyId)))},
  {"UserID", AstraValueParser(asString(FIELD_ACCESS(userId)))},

  // Boat motion
  {"BS", AstraValueParser()},



  // TODO: Because the log file typically contains some sort of date,
  // could we use that date to determine what unit to use for COG and SOG?
  // The date is also stored in the header at the top of the file. So
  // we can decide on the units before we have even started parsing the data.
  //
  // This value could be unreliable, See message2.txt:
  //   "'COG' in radians (but it will be in degrees like GWD and TWD);"
  {"COG", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(COG)))},

  // This value could be unreliable, See message2.txt:
  //   "'SOG' in m/s (but it will be in knots);"
  {"SOG", AstraValueParser(inUnit(1.0_mps, FIELD_ACCESS(SOG)))},

  // Boat position

  // Should be some kind of fractional degrees here.
  {"Lat", AstraValueParser(geographicAngle(
      'N', 'S', FIELD_ACCESS(lat)))},
  {"Lon", AstraValueParser(geographicAngle(
      'E', 'W', FIELD_ACCESS(lon)))},
  {"Latitudine", AstraValueParser(geographicAngle(
      'N', 'S', FIELD_ACCESS(lat)))},
  {"Longitudine", AstraValueParser(geographicAngle(
      'E', 'W', FIELD_ACCESS(lon)))},

  // Should be unit degrees here? Not sure. In message2.txt, it says
  //
  // "  'LAT' and 'LON' is like google maps format, so decimals of degrees (but it will be as explained just before"
  //
  // But looking at the log file data, it seems that degrees is the unit
  // being used.
  {"LAT", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(lat)))},
  {"LON", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(lon)))},

  // Other boat orientation
  {"HDG", AstraValueParser()},
  {"Pitch", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(pitch)))},
  {"Roll", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(roll)))},

  {"AW_angle", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(AWA)))},
  {"AW_speed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(AWS)))},
  {"BS_polar",AstraValueParser()},
  {"BS_target",AstraValueParser()},
  {"Boatspeed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(waterSpeed)))},
  {"Ext_COG", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(COG)))},
  {"Ext_SOG", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(SOG)))},
  {"Heading", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(magHdg)))},
  {"LeewayAng",AstraValueParser()},
  {"LeewayMod",AstraValueParser()},
  {"TWA_target",AstraValueParser()},
  {"TW_Dir", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWD)))},
  {"TW_angle", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWA)))},
  {"TW_speed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(TWS)))},
  {"Type_tgt",AstraValueParser()}
*/


//// How the exported ESA values are rendered
var renderEsaDate = renderEsaUndefined;
var renderEsaTime = renderEsaUndefined;
var renderEsaTs = renderEsaUndefined;
var renderEsaBoatspeed = renderEsaUndefined;
var renderEsaAW_angle = renderEsaUndefined;
var renderEsaAW_speed = renderEsaUndefined;
var renderEsaHeading = renderEsaUndefined;
var renderEsaTW_angle = renderEsaUndefined;
var renderEsaTW_speed = renderEsaUndefined;
var renderEsaTW_Dir = renderEsaUndefined;
var renderEsaExt_SOG = renderEsaUndefined;
var renderEsaExt_COG = renderEsaUndefined;
var renderEsaLatitudine = renderEsaUndefined;
var renderEsaLongitudine = renderEsaUndefined;
var renderEsaBS_target = renderEsaUndefined;
var renderEsaTWA_target = renderEsaUndefined;
var renderEsaBS_polar = renderEsaUndefined;
var renderEsaType_tgt = renderEsaUndefined;
var renderEsaLeewayAng = renderEsaUndefined;
var renderEsaLeewayMod = renderEsaUndefined;
var renderEsaSET = renderEsaUndefined;
var renderEsaDRIFT = renderEsaUndefined;

//// Declaration of all the columns to be exported for the ESA log file format
var esaColumns = [{
  esaName: 'Date',
  render: renderEsaDate
}, {
  esaName: 'Time',
  render: renderEsaTime
}, {
	esaName: 'Ts',
  render: renderEsaTs
}, {
  esaName: 'Boatspeed',
  render: renderEsaBoatspeed
}, {
  esaName: 'AW_angle',
  render: renderEsaAW_angle
}, {
  esaName: 'AW_speed',
  render: renderEsaAW_speed
}, {
	esaName: 'Heading',
  render: renderEsaHeading
}, {
  esaName: 'TW_angle',
  render: renderEsaTW_angle
}, {
  esaName: 'TW_speed',
  render: renderEsaTW_speed
}, {
	esaName: 'TW_Dir',
  render: renderEsaTW_Dir
}, {
	esaName: 'Ext_SOG',
  render: renderEsaExt_SOG
}, {
  esaName: 'Ext_COG',
  render: renderEsaExt_COG
}, {
  esaName: 'Latitudine',
  render: renderEsaLatitudine
}, {
  esaName: 'Longitudine',
  render: renderEsaLongitudine
}, {
  esaName: 'BS_target',
  render: renderEsaBS_target
}, {
  esaName: 'TWA_target',
  render: renderEsaTWA_target
}, {
	esaName: 'BS_polar',
  render: renderEsaBS_polar
}, {
  esaName: 'Type_tgt',
  render: renderEsaType_tgt
}, {
  esaName: 'LeewayAng',
  render: renderEsaLeewayAng
}, {
	esaName: 'LeewayMod',
  render: renderEsaLeewayMod
}, {
  esaName: 'SET',
  render: renderEsaSET
}, {
  esaName: 'DRIFT',
  render: renderEsaDRIFT
}];

function sendEsaHeader(res, columns) {
  //var row = [ "DATE/TIME(UTC)" ].concat(columns).map(csvEscape);
  //res.write(row.join(',') + '\n');
  res.write('ESA HEADER goes here!!!\n');
}

function sendEsaChunk(res, columns, table, columnType) {
  res.write('ESA CHUNK goes here!!!\n');
}




 // These values should match those in CharTiles.h
const minZoom = 9;
const maxZoom = 28;
const samplesPerTile = 512;

function pad(num, size) {
  var s = num+"";
  while (s.length < size) s = "0" + s;
  return s;
}

function formatTime(date) {
  // 06/20/2018 4:26:35 PM
  return strftime("%m/%d/%Y %l:%M:%S %p", date);
  /* The following is the reference implementation, but it is too slow.
  return date.toLocaleString('en-US', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: true,
    timeZone: 'UTC'
  }).replace(/,/, '');
  */
}

function getColumn (columns, title) {
  if (title in columns) {
    return columns[title];
  }
  return columns[title] = Object.keys(columns).length;
}

// Returns a row in the table as an array, 
// or allocates a new array if the row does not exist.
function getRow(table, timeSec) {
  var t = timeSec + '';
  if (!(t in table)) {
    table[t] = [ ];
  }
  return table[t];
}

function csvEscape(s) {
  if (!s) {
    return '"-"';
  }
  return '"' + s.replace(/"/g, '""').replace(/,/g,';') + '"';
}

function formatNumber(s, decimals) {
  s = s + '';
  if (s.match(/^[ ]*[-]?\d+\.\d+$/)) {
    var fixed = parseFloat(s).toFixed(decimals);
    return fixed.match(/(.*?)\.?0*$/)[1];
  }
  return s;
}

function formatColumnEntry(type, entry) {
  if (entry == undefined) {
    return '';
  }

  switch (type) {
    case 'longitude':
    case 'latitude':
      return entry;
    case 'awa':
    case 'twa':
    case 'gpsBearing':
    case 'magHdg':
    case 'twdir':
    case 'vmg':
    case 'targetVmg':
      return formatNumber(entry, 1);
    case 'aws':
    case 'tws':
    case 'rudderAngle':
    case 'waterSpeed':
    case 'gpsSpeed':
      return formatNumber(entry, 2);
    default:
      return formatNumber(entry, 2);
  }
}


function sendCsvHeader(res, columns) {
  var row = [ "DATE/TIME(UTC)" ].concat(columns).map(csvEscape);
  res.write(row.join(',') + '\n');
}

function sendCsvChunk(res, columns, table, columnType) {
  var numCols = 1 + columns.length;

  var times = Object.keys(table);
  times.sort(function(a, b) { return parseInt(a) - parseInt(b); });

  for (var time_index = 0 ; time_index < times.length; ++ time_index) {
    var t = times[time_index];

    var rowDate = new Date(parseInt(t) * 1000);

    row = [];
    row[0] = formatTime(rowDate);
    var tableRow = table[t];
    for (var i = 0; i < numCols; ++i) {
      row[i + 1] = formatColumnEntry(columnType[i], tableRow[i]); 
    }
    res.write(row.join(', ') + '\n');
  }
}

var csvFormat = {
  contentType: 'text/csv',
  sendHeader: sendCsvHeader,
  sendChunk: sendCsvChunk,
  fileExtension: ".csv"
};

var esaFormat = {
  contentType: 'text/plain',
  sendHeader: sendEsaHeader,
  sendChunk: sendEsaChunk,
  fileExtension: ".log"
};


var listChannelsWithSources = function(boat, zoom, firstTile, lastTile, cb) {
  ChartTiles.aggregate([
    {
      $match: {
        boat: ObjectId(boat),
        zoom: zoom,
        tileno: { $gte: firstTile, $lte: lastTile }
      },
    }, {
      $group: {
        _id: { what: "$what", source: "$source" },
        total: { $sum: 1 }
      }
    }, {
      $group: {
        _id: "$_id.what",
        sources: { $push: "$_id.source" }
      }
    }, {
      $project: {
        _id: false,
        what: "$_id",
        sources: true
      }
    }, {
      $sort: {what: 1}
    }
  ]).exec(function(err, data) {
    if (err) {
      cb(err);
      return;
    }
    ChartSource.findById(boat, (err, chartSources) => {
      if (err) {
        cb(err);
        return;
      }
      cb(null, data.map((x) => {
        return {
          what: x.what,
          sources: x.sources.map((source) => {
            return {
              source: source,
              prio: prioOfSource(x.what, source, chartSources)
            };
          }).sort((a, b) => b.prio - a.prio)
        };
      }));
    });
  });
};

// Finds the most suitable source for a given channel.
//
// Example usage:
//
//  ChartSource.findById(boat, (err, chartSources) => {
//    if (err) {
//      // handle error
//      return;
//    }
//    const gpsSpeedSource = bestSourceForChannel("gpsSpeed", chartSources);
//    const latitudeSource = bestSourceForChannel("latitude", chartSources);
//    const longitudeSource = bestSourceForChannel("longitude", chartSources);
//  });
function bestSourceForChannel(channel, chartSources) {
  const sources = Object.keys(chartSources.channels[channel]);
  let bestPrio;
  let bestSource;
  for (let s of sources) {
    let prio = chartSources.channels[channel][s].priority
      + (s.match(/mix/) ? .1 : 0);
    if (bestPrio == undefined || prio > bestPrio) {
      bestPrio = prio;
      bestSource = s;
    }
  }
  return bestSource;
}

var listColumns = function(boat, zoom, firstTile, lastTile, cb) {
  ChartTile.aggregate([
    {
      $match: {
        boat: ObjectId(boat),
        zoom: zoom,
        tileno: { $gte: firstTile, $lte: lastTile }
      },
    }, {
      $group: {
        _id: { what: "$what", source: "$source" },
      }
    }, {
      $sort: { _id: 1 }
    }
  ]).exec(function(err, res) {
    if (err) {
      cb(err);
    } else {
      cb(undefined, res.map(function(e) {
        return e._id.what + ' - ' + e._id.source;
      }));
    }
  });
};
 
function sendWithColumns(
  outputFormat,
  chartSources,
  start, end, boat, zoom, firstTile, lastTile,
  columns, res, timeRange) {

  var query = {
    boat: mongoose.Types.ObjectId(boat),
    zoom: zoom,
    tileno: {
      "$gte": firstTile,
      "$lte": lastTile
    }
  };

  console.warn(query);

  // Maps a time (in seconds) to arrays of mean values
  var table = { };

  // Maps a column index to a channel source name
  var columnType = { };

  var resultSent = false;

  var currentTile = firstTile;

  res.contentType(outputFormat.contentType);
  res.header("Content-Disposition", "attachment;filename=" + timeRange + outputFormat.fileExtension);
  outputFormat.sendHeader(res, columns);

  // The query bypasses mongoose.
  ChartTile.collection
    .find(query)
    // see https://docs.mongodb.com/manual/tutorial/sort-results-with-indexes/
    .sort({ boat:1, zoom:1, tileno: 1})
    .forEach(function(packedTile) {
      var tile = expandArrays(packedTile);

      var columnTitle = tile.what + ' - ' + tile.source;
      var firstTime = new Date(1000 * tile.tileno * (1 << tile.zoom));

      // Should we complete this chunk and move on?
      if (tile.tileno > currentTile) {
        // Send what we have.
        outputFormat.sendChunk(res, columns, table, columnType);
        currentTile = tile.tileno;

        // Reset them, so that we can start over.
        table = { };
        columnType = { };
      }

      // increment between samples in s
      var increment = (1 << tile.zoom) / samplesPerTile;
      var firstTimeSec = Math.floor(firstTime / 1000);
      var startTimeSec = start.getTime() / 1000;
      var endTimeSec = end.getTime() / 1000;

      var colno = columns.indexOf(columnTitle);
      if (colno < 0) {
        return;
      }
      columnType[colno] = tile.what;

      for (var i = 0; i < samplesPerTile; ++i) {
        if (tile.count && tile.count[i] > 0) {
          var time = firstTimeSec + i * increment;
          if (time > startTimeSec && time < endTimeSec) {

            // Populate the table row
            var row = getRow(table, time);
            row[colno] = tile.mean[i];
          }
        }
      }

      if (resultSent) {
        console.warn('ERROR! received DB data to build an reply that has been sent already!');
        console.warn(new Error().stack);
      }
    },
    function(err) {
      if (err) {
        res.status(500).send();
      } else {
        setTimeout(function() {
          resultSent = true;
          outputFormat.sendChunk(res, columns, table, columnType);
          res.status(200).end();
        }, 1);
      }
    });
}






exports.exportCsv = function(req, res, next) {
  var timeRange = req.params.timerange;
  var boat = req.params.boat + '';
  if (typeof(timeRange) != 'string' || timeRange.length != 2 * dateLength) {
    return res.status(400).send();
  }
  var start = new Date(timeRange.substr(0, dateLength) + 'Z');
  var end = new Date(timeRange.substr(dateLength, dateLength) + 'Z');
  if (isNaN(start.getTime()) || isNaN(end.getTime())) {
    return res.status(400).send();
  }

  // Fixed for now, might be configurable by the user later.
  var frequency = 1; // Hz

  var zoom = Math.log2(samplesPerTile / frequency)

  var tileNo = function(date, rounding) {
    return Math[rounding](date.getTime() / 1000 / (1 << zoom));
  };

  var firstTile = tileNo(start, 'floor');
  var lastTile = tileNo(end, 'ceil');

  ChartSource.findById(boat, function(err, chartSources) {
    if (err) {
      console.warn(err);
      res.status(404).send();
    } else {

      console.log('Chart sources: %j', chartSources);

      listColumns(
        boat, zoom, firstTile, lastTile,
        function(err, columns) {
          if (err) {
            console.warn(err);
            res.status(500).send();
          } else if (columns.length == 0) {
            res.status(404).send();
          } else {
            sendWithColumns(
              csvFormat, // This is where the format is decided: csvFormat or esaFormat
              chartSources,
              start, end, boat, zoom, firstTile, lastTile,
              columns, res, timeRange);
          }
        });
    }
  });
};
