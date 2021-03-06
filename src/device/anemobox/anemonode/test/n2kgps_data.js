var nmea0183 = [
    "$GNRMC,163433.00,A,4629.60710,N,00640.91834,E,7.105,237.70,190615,,,A*74\n"
  + "$GNVTG,237.70,T,,M,7.105,N,13.159,K,A*1E\n"
  + "$GNGGA,163433.00,4629.60710,N,00640.91834,E,1,12,0.63,375.2,M,47.2,M,,*4A\n"
  + "$GNGSA,A,3,16,18,20,29,26,22,31,27,21,,,,1.20,0.63,1.03*1F\n"
  + "$GNGSA,A,3,85,76,69,84,70,86,74,75,68,,,,1.20,0.63,1.03*11\n"
  + "$GPGSV,4,1,15,05,01,025,,07,00,340,,08,65,161,35,16,59,302,39*70\n"
  + "$GPGSV,4,2,15,18,30,138,33,19,04,271,33,20,27,059,26,21,67,070,31*7B\n"
  + "$GPGSV,4,3,15,22,10,169,30,26,77,213,38,27,33,282,36,29,19,084,24*78\n"
  + "$GPGSV,4,4,15,31,14,206,32,33,32,209,,39,34,155,*49\n"
  + "$GLGSV,3,1,09,68,13,021,37,69,28,073,17,70,13,128,33,74,16,188,29*62\n"
  + "$GLGSV,3,2,09,75,52,238,36,76,35,319,32,84,36,054,25,85,67,335,35*69\n"
  + "$GLGSV,3,3,09,86,28,270,32*5C\n"
    + "$GNGLL,4629.60710,N,00640.91834,E,163433.00,A,A*7B\n", 
    "$GNRMC,073759.00,V,,,,,,,030615,,,N*6D\n"
  + "$GNVTG,,,,,,,,,N*2E\n"
  + "$GNGGA,073759.00,,,,,0,04,13.44,,,,,,*71\n"
  + "$GNGSA,A,1,15,21,24,,,,,,,,,,15.49,13.44,7.71*15\n"
  + "$GNGSA,A,1,79,,,,,,,,,,,,15.49,13.44,7.71*1A\n"
  + "$GPGSV,2,1,06,05,,,39,13,,,31,15,46,295,37,20,,,37*49\n"
  + "$GPGSV,2,2,06,21,14,299,32,24,10,242,36*7C\n"
  + "$GLGSV,1,1,02,79,33,222,36,,,,35*58\n"
    + "$GNGLL,,,,,073759.00,V,N*5B\n"];  

var n2kgps = require('../components/n2kgps.js');


module.exports = nmea0183.map(function(raw) {
  return {
    raw: raw,
    packets: n2kgps.makePackets(raw)
  };
});
