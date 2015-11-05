
#include <server/nautical/tiles/NavTileUploader.h>

#include <device/Arduino/libraries/TrueWindEstimator/TrueWindEstimator.h>
#include <mongo/client/dbclient.h>
#include <mongo/bson/bson-inl.h>
#include <server/common/logging.h>
#include <server/nautical/tiles/NavTileGenerator.h>

using namespace mongo;

#if MONGOCLIENT_VERSION_MAJOR < 1
#define MONGO_QUERY QUERY
namespace mongo { namespace client { void initialize() { } } }
#endif

namespace sail {

namespace {
BSONObjBuilder& append(BSONObjBuilder& builder, const char* key,
                       const TimeStamp& value) {
  return builder.appendDate(key, Date_t(value.toMilliSecondsSince1970()));
}

BSONObj navToBSON(const Nav& nav) {
  BSONObjBuilder result;

  // GPS is mandatory.
  append(result, "time", nav.time());
  result.append("pos",
                BSON_ARRAY(posToTileX(0, nav.geographicPosition())
                           << posToTileY(0, nav.geographicPosition())));
  result.append("gpsBearing", nav.gpsBearing().degrees());
  result.append("gpsSpeed", nav.gpsSpeed().knots());

  if (nav.hasApparentWind()) {
    result.append("awa", nav.awa().degrees());
    result.append("aws", nav.aws().knots());
  }
  if (nav.hasMagHdg()) {
    result.append("magHdg", nav.magHdg().degrees());
  }
  if (nav.hasWatSpeed()) {
    result.append("watSpeed", nav.watSpeed().knots());
  }
  if (nav.hasExternalTrueWind()) {
    result.append("externalTwa", nav.externalTwa().degrees());
    result.append("externalTws", nav.externalTws().knots());
  }
  if (nav.hasTrueWindOverGround()) {
    result.append("twdir", calcTwdir(nav.trueWindOverGround()).degrees());
    result.append("tws", calcTws(nav.trueWindOverGround()).knots());
  }

  // Old anemobox simulated data.
  if (nav.hasDeviceScreen()) {
    result.append("devicePerf", nav.deviceScreen().perf);
    result.append("deviceTwdir", nav.deviceScreen().twdir);
    result.append("deviceTws", nav.deviceScreen().tws);
  }

  // New anemobox logged data.
  if (nav.hasDeviceVmg()) {
    result.append("deviceVmg", nav.deviceVmg().knots());
  }
  if (nav.hasDeviceTargetVmg()) {
    result.append("deviceTargetVmg", nav.deviceTargetVmg().knots());
  }
  return result.obj();
}

BSONArray navsToBSON(const Array<Nav>& navs) {
  BSONArrayBuilder result;
  for (auto nav: navs) {
    result.append(navToBSON(nav));
  }
  return result.arr();
}

bool safeMongoOps(std::string what,
    DBClientConnection *db, std::function<void(DBClientConnection*)> f) {
  try {
    f(db);
    std::string err = db->getLastError();
    if (err != "") {
      LOG(ERROR) << "error while " << what << ": " << err;
      return false;
    }
  } catch (const DBException &e) {
    LOG(ERROR) << "error while " << what << ": " << e.what();
    return false;
  }
  return true;
}

bool insertOrUpdateTile(const BSONObj& obj,
    const TileGeneratorParameters& params,
                        DBClientConnection* db) {
  if (!params.fullClean) {
    safeMongoOps("cleaning old tiles",
        db, [=](DBClientConnection *db) {
      db->remove(params.tileTable(),
                 MONGO_QUERY("key" << obj["key"]
                       << "boat" << obj["boat"]
                       << "startTime" << GTE << obj["startTime"]
                       << "endTime" << LTE << obj["endTime"]));
    });
  }
  return safeMongoOps("inserting a tile in mongoDB",
      db, [=](DBClientConnection *db) {
    db->insert(params.tileTable(), obj);
  });
}

bool insertSession(const BSONObj &obj,
  const TileGeneratorParameters& params,
  DBClientConnection *db) {
  return safeMongoOps("updating a session", db,
    [=](DBClientConnection *db) {
    db->update(params.sessionTable(),// <-- The collection
        MONGO_QUERY("_id" << obj["_id"]),  // <-- what to update
        obj,                         // <-- the new data
        true,                        // <-- upsert
        false);                      // <-- multi
  });
}

template <typename T>
Angle<T> average(const Angle<T>& a, const Angle<T>& b) {
  HorizontalMotion<T> motion =
    HorizontalMotion<T>::polar(Velocity<T>::knots(1), a);
  motion = motion + HorizontalMotion<T>::polar(Velocity<T>::knots(1), b);
  return motion.angle();
}

BSONObj locationForSession(const Array<Nav>& navs) {
  if (navs.size() == 0) {
    return BSONObj();
  }

  Angle<double> minLat(navs[0].geographicPosition().lat());
  Angle<double> minLon(navs[0].geographicPosition().lon());
  Angle<double> maxLat(navs[0].geographicPosition().lat());
  Angle<double> maxLon(navs[0].geographicPosition().lon());
  
  for (auto nav: navs) {
    minLat = std::min(minLat, nav.geographicPosition().lat());
    maxLat = std::max(maxLat, nav.geographicPosition().lat());
    minLon = std::min(minLon, nav.geographicPosition().lon());
    maxLon = std::max(maxLon, nav.geographicPosition().lon());
  }

  GeographicPosition<double> center(
      average(minLon, maxLon), average(minLat, maxLat));

  GeographicPosition<double> minPos(minLon, minLat);
  GeographicPosition<double> maxPos(maxLon, maxLat);

  BSONObjBuilder location;
  location.append("x", posToTileX(0, center));
  location.append("y", posToTileY(0, center));
  location.append("scale", 2 * std::max(
          posToTileX(0, maxPos) - posToTileX(0, minPos),
          posToTileY(0, maxPos) - posToTileY(0, minPos)));
  return location.obj();
}

BSONObj makeBsonSession(
    const std::string &curveId,
    const std::string &boatId,
    Array<Nav> navs) {

  BSONObjBuilder session;
  session.append("_id", curveId);
  session.append("boat", OID(boatId));
  session.append("trajectoryLength",
      computeTrajectoryLength(navs).nauticalMiles());
  int maxSpeedIndex = findMaxSpeedOverGround(navs);
  if (maxSpeedIndex >= 0) {
    session.append("maxSpeedOverGround", navs[maxSpeedIndex].gpsSpeed().knots());
    append(session, "maxSpeedOverGroundTime", navs[maxSpeedIndex].time());
  }
  append(session, "startTime", navs.first().time());
  append(session, "endTime", navs.last().time());
  session.append("location", locationForSession(navs));

  return session.obj();
}

BSONObj makeBsonTile(const TileKey& tileKey,
                     const Array<Array<Nav>>& subCurvesInTile,
                     const std::string& boatId,
                     const std::string& curveId) {
  BSONObjBuilder tile;
  tile.genOID();
  tile.append("key", tileKey.stringKey());
  tile.append("boat", OID(boatId));
  append(tile, "startTime", subCurvesInTile.first().first().time());
  append(tile, "endTime", subCurvesInTile.last().last().time());
  append(tile, "created", TimeStamp::now());

  std::vector<BSONObj> curves;
  for (auto subCurve: subCurvesInTile) {
    BSONObjBuilder subCurveBuilder;

    subCurveBuilder
      .append("curveId", curveId)
      .append("points", navsToBSON(subCurve));

    curves.push_back(subCurveBuilder.obj());
  }
  tile.append("curves", curves);
  return tile.obj();
}

}  // namespace

bool generateAndUploadTiles(std::string boatId,
                            Array<Array<Nav>> allNavs,
                            const TileGeneratorParameters& params) {
  mongo::client::initialize();

  DBClientConnection db;
  std::string err;
  if (!db.connect(params.dbHost, err)) {
    LOG(ERROR) << "mongoDB connection failed: " << err;
    return false;
  }

  if (params.fullClean) {
    db.remove(params.tileTable(),
               MONGO_QUERY("boat" << OID(boatId)));
  }

  for (const Array<Nav>& curve : allNavs) {
    std::string curveId = tileCurveId(boatId, curve);

    std::set<TileKey> tiles = tilesForNav(curve, params.maxScale);

    for (auto tileKey : tiles) {
      Array<Array<Nav>> subCurvesInTile = generateTiles(
          tileKey, curve, params.maxNumNavsPerSubCurve);

      if (subCurvesInTile.size() == 0) {
        continue;
      }

      BSONObj tile = makeBsonTile(tileKey, subCurvesInTile, boatId, curveId);

      if (!insertOrUpdateTile(tile, params, &db)) {
        // There is no point to continue if we can't write to the DB.
        return false;
      }
    }
    BSONObj session = makeBsonSession(curveId, boatId, curve);
    if (!insertSession(session, params, &db)) {
      return false;
    }
  }
  return true;
}

}  // namespace sail

