(function(exports){

  function push(dst, x) {
    dst.push(x);
    return dst;
  }

  function fatalError(x) {
    alert('FATAL ERROR: %j', x); 
  }

  function assert(x, msg) {
    if (!x) {
      fatalError("Assertion failed: '%s'", msg);
    }
  }

  function allocateFieldIfNeeded(obj, key) {
    if (!(key in obj)) {
      obj[key] = {};
    }
  }

  // Set a value deeply in a datastructure
  function setIn(dst, path, value) {
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
  function getIn(src, path) {
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
  function updateIn(dst, path, f) {
    // Not optimized, but simple.
    // TODO: Optimize it.
    return setIn(dst, path, f(getIn(dst, path)));
  }

  exports.getIn = getIn;
  exports.setIn = setIn;
  exports.updateIn = updateIn;
  exports.assert = assert;
  exports.fatalError = fatalError;
  exports.push = push;

})(typeof exports === 'undefined'? this['anemoutils']={}: exports);
