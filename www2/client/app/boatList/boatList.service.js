'use strict';

angular.module('www2App')
  .service('boatList', function (Auth, $http, $q,socket, $rootScope) {
    var boats = [ ];
    var boatDict = { };
    var curves = { };
    var sessionsForBoats = {};

    //
    // promise for boats methods;
    var deferred = $q.defer();
    var promise=deferred.promise;


    //
    // return the default boat to display at home
    function getDefaultBoat() {
      if(!boats.length){
        return {};
      }

      // default super implementation
      return boats[boats.length-1];
    }

    function update() {
      //
      // if this promise is already deferred
      if(promise.$$state.status){
        deferred = $q.defer();  
        promise=deferred.promise;      
      }

      // Specifically ask for public boat, too.
      // We could have a user option to hide public boats.
      $http.get('/api/boats?public=1')
        .then(function(payload) {
          boats = payload.data;
          socket.syncUpdates('boat', boats);

          for (var i in payload.data) {
            var boat = payload.data[i];
            boatDict[boat._id] = boat;
          }
          $rootScope.$broadcast('boatList:updated', boats);

          //
          // chain session loading
          return $http.get('/api/session');
        })
        .then(function(payload) {
          sessionsForBoats = [];
          for (var i in payload.data) {
            if (payload.data[i].boat in sessionsForBoats) {
              sessionsForBoats[payload.data[i].boat].push(payload.data[i]);
            } else {
              sessionsForBoats[payload.data[i].boat] = [ payload.data[i] ];
            }
            curves[payload.data[i]._id] = payload.data[i];
          }
          //
          // time to resolve promise
          $rootScope.$broadcast('boatList:sessionsUpdated', sessionsForBoats);
          deferred.resolve(boats);            
        });

      return promise;
    }


    function locationForCurve(curveId) {
      if (!(curveId in curves)) {
        return undefined;
      }
      var c = curves[curveId];
      return c.location;
    }

    //
    // init
    update();
    $rootScope.$watch(Auth.isLoggedIn, function(newVal, oldVal) {
      if (newVal && newVal != oldVal) {
        update();    
      }
    });

    //
    // service result
    return {
      boat: function(id) { return boatDict[id]; },
      boats: function() { return promise; },
      sessions: function() { return $.extend({}, sessionsForBoats); },
      sessionsForBoat: function(boatId) { return sessionsForBoats[boatId]; },
      getCurveData: function(curveId) { return curves[curveId]; },
      getDefaultBoat: getDefaultBoat,
      locationForCurve: locationForCurve,
      update: update,
    };
  });
