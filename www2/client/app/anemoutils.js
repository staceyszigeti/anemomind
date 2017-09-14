(function(exports){

  exports.fatalError = function (x) {
    alert('FATAL ERROR: %j', x); 
  }

  exports.assert = function(x, msg) {
    if (!x) {
      exports.fatalError("Assertion failed: '%s'", msg);
    }
  }

  function allocateFieldIfNeeded(obj, key) {
    if (!(key in obj)) {
      obj[key] = {};
    }
  }

  // Set a value deeply in a datastructure
  exports.setIn = function(dst, path, value) {
    if (path.length == 0) {
      return value;
    }
    var root = dst || {};
    var obj = root;
    var last = path.length - 1;
    for (var i = 0; i < last; i++) {
      var key = path[i];
      allocateFieldIfNeeded(obj, key);
      obj = obj[key];
    }
    var key = path[last];
    obj[key] = value;
    return root;
  };

  // Get a value from deep inside a datastructure
  exports.getIn = function(src, path) {
    var obj = src;
    for (var i in path) {
      if (!obj) {
        return null;
      }
      var key = path[i];
      obj = obj[key];
    }
    return obj;
  };

  // Update a value in a deep data structure
  exports.updateIn = function(dst, path, f) {
    // Not optimized, but simple.
    // TODO: Optimize it.
    return exports.setIn(dst, path, f(exports.getIn(dst, path)));
  }

})(typeof exports === 'undefined'? this['anemoutils']={}: exports);
