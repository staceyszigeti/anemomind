/**
* Reference: https://developer.bluetooth.org/gatt/services/Pages/ServiceViewer.aspx?u=org.bluetooth.service.device_information.xml
*/

var util = require('util'),
  exec = require('child_process').exec,
  bleno = require('bleno'),
  Descriptor = bleno.Descriptor,
  Characteristic = bleno.Characteristic,
  boxId = require('./boxId'),
  anemoId;

  boxId.getAnemoId(function(id) {
    anemoId = new Buffer(id, 'utf8');
  });
/**
* Reference:
* https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.manufacturer_name_string.xml
*/
var DeviceManufacturerCharacteristic = function() {
  DeviceManufacturerCharacteristic.super_.call(this, {
      // Device Manufacturer
      uuid: '2A29',
      properties: ['read'],
      value: anemoId,
      descriptors: [
        new Descriptor({
        uuid: '2901',
        value: 'Device Manufacturer'
      })]
  });
};

util.inherits(DeviceManufacturerCharacteristic, Characteristic);

module.exports = DeviceManufacturerCharacteristic;
