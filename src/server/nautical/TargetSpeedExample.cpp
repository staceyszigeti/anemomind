/*
 *  Created on: 2014-05-20
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/nautical/TargetSpeed.h>
#include <server/common/Env.h>
#include <server/common/PathBuilder.h>
#include <server/nautical/NavNmeaScan.h>
#include <server/common/Span.h>
#include <iostream>
#include <server/plot/extra.h>
#include <server/nautical/grammars/Grammar001.h>

using namespace sail;

namespace {
  void targetSpeedPlot() {
    Poco::Path srcpath = PathBuilder::makeDirectory(Env::SOURCE_DIR).
        pushDirectory("datasets").
        pushDirectory("regates").get();
    Array<Nav> allnavs = scanNmeaFolder(srcpath, Nav::debuggingBoatId());

    Grammar001Settings settings;
    Grammar001 g(settings);


    std::shared_ptr<HTree> tree = g.parse(allnavs);

    Arrayb upwind = markNavsByDesc(tree, g.nodeInfo(), allnavs, "upwind-leg");
    assert(!upwind.empty());
    assert(countTrue(upwind) > 0);


    Array<Nav> upwindNavs = allnavs.slice(upwind);

    const int binCount = 25;
    Array<Velocity<double> > tws = estimateRawTws(upwindNavs);
    Array<Velocity<double> > vmg = calcUpwindVmg(upwindNavs);
    Array<Velocity<double> > gss = gpsSpeed(upwindNavs);

    Arrayd gssd = gss.map<double>([&](Velocity<double> x) {return x.metersPerSecond();});
    Arrayd twsd = tws.map<double>([&](Velocity<double> x) {return x.metersPerSecond();});
    Arrayd vmgd = vmg.map<double>([&](Velocity<double> x) {return x.metersPerSecond();});

    std::cout << "GPS-span (m/s): " << Spand(gssd) << std::endl;
    std::cout << "TWS-span (m/s): " << Spand(twsd) << std::endl;
    std::cout << "VMG-span (m/s): " << Spand(vmgd) << std::endl;

    Velocity<double> minvel = Velocity<double>::metersPerSecond(4.0);
    Velocity<double> maxvel = Velocity<double>::metersPerSecond(17.0);
    TargetSpeedData tgt(tws, vmg, HistogramMap(binCount,
        minvel.metersPerSecond(), maxvel.metersPerSecond()));

    tgt.plot();
  }
}

int main() {
  targetSpeedPlot();
  return 0;
}
