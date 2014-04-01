/*
 *  Created on: 28 mars 2014
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "TimeStampJson.h"
#include <server/common/TimeStamp.h>
#include <server/common/logging.h>

namespace sail {
namespace json {

namespace {
  const char TimeLabel[] = "milliseconds-since-1970";

  std::string makeFname(std::string prefix) {
    return prefix + "-" + TimeLabel;
  }
}

bool deserializeField(Poco::JSON::Object::Ptr obj, std::string prefix, TimeStamp *out) {
  std::string fname = makeFname(prefix);
  if (obj->has(fname)) {
    *out = TimeStamp::fromMilliSecondsSince1970(obj->getValue<Poco::Int64>(fname));
    return true;
  }
  *out = TimeStamp::makeUndefined();
  return false;
}


Poco::JSON::Object::Ptr serialize(const TimeStamp &src) {
  Poco::JSON::Object::Ptr obj(new Poco::JSON::Object());
  obj->set("time-value", src.toMilliSecondsSince1970());
  obj->set("time-label", "milliseconds-since-1970");
  return obj;
}

void deserialize(Poco::JSON::Object::Ptr src, TimeStamp *dst) {
  CHECK(src->get("time-label").convert<std::string>() == TimeLabel);
  *dst = TimeStamp::fromMilliSecondsSince1970(src->getValue<int64_t>("time-value"));
}






void serializeField(Poco::JSON::Object::Ptr obj, std::string prefix, const TimeStamp &x) {
  obj->set(makeFname(prefix), static_cast<Poco::Int64>(x.toMilliSecondsSince1970()));
}

}  // namespace json
}  // namespace sail
