/* julien@anemomind.com, 4.2015
 *
 * Nodejs interface to Dispatcher, Nmea0183Source, etc..
 *
 * See README.md for usage information.
 */
#include <node.h>
#include <nan.h>

#include <device/anemobox/anemonode/JsDispatcher.h>
#include <device/anemobox/anemonode/JsNmea0183Source.h>
#include <device/anemobox/anemonode/JsEstimator.h>
#include <device/anemobox/anemonode/JsLogger.h>

#include <iostream>
#include <vector>

#include <sys/time.h>

using namespace sail;
using namespace v8;
using namespace node;

namespace {

NAN_METHOD(adjTime) {
  NanScope();

  if (!args[0]->IsNumber()) {
    NanThrowTypeError("Expecting time delta in seconds");
    NanReturnUndefined();
  }
  double delta = args[0]->ToNumber()->Value();
  struct timeval tv;
  gettimeofday(&tv, 0);
  double secs = double(tv.tv_sec) + double(tv.tv_usec)*1e-6 + delta;
  tv.tv_sec = time_t(secs);
  tv.tv_usec = (secs - tv.tv_sec) * 1e6;
  int r = settimeofday(&tv, 0);
  if (r) {
    perror("settimeofday");
  }
  NanReturnValue(r);
}

NAN_METHOD(currentTime) {
  NanScope();

  //TimeStamp now = Dispatcher::global()->currentTime();
  TimeStamp now = TimeStamp::now();
  NanReturnValue(NanNew<Date>(now.toMilliSecondsSince1970()));
}

}  // namespace

void RegisterModule(Handle<Object> target) {
  JsDispatcher::Init(Dispatcher::global(), target);
  JsNmea0183Source::Init(target);
  JsLogger::Init(target);
  JsEstimator::Init(target);

  NODE_SET_METHOD(target, "adjTime", adjTime);
  NODE_SET_METHOD(target, "currentTime", currentTime);
}

// Register the module with node. Note that "modulename" must be the same as
// the basename of the resulting .node file. You can specify that name in
// binding.gyp ("target_name"). When you change it there, change it here too.
NODE_MODULE(anemonode, RegisterModule);
