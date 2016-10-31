'use strict';

angular.module('www2App')
  .controller('BoatsCtrl', function ($scope, $http, $stateParams,$location, socket, boatList,Auth,$log) {
    $scope.isLoggedIn = Auth.isLoggedIn();

    $scope.$watch(Auth.isLoggedIn, function(newVal, oldVal) {
      $scope.isLoggedIn = newVal;
    });

    
    $log.log('-- boat.ctrl.0');

    // TODO: only load the boat we want, not all boats.
    boatList.boats().then(function(boats) {
      $scope.boats = boats;

      $log.log('-- boat.ctrl.1',boats.length);

      // display selected boat
      $scope.boatId=$stateParams.boatId;
      if(!$scope.boatId && $location.path()==='/'){
        //
        // display default boat
        $scope.boatId=(boatList.getDefaultBoat() || {})._id;        
      }
    });


    $scope.addBoat = function() {
      if(!$scope.newBoat||$scope.newBoat === '') {
        return;
      }
      $http.post('/api/boats', { name: $scope.newBoat });
      $scope.newBoat = '';
      boatList.update();
    };


  });
