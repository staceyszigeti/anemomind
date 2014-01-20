/*
 * extra.cpp
 *
 *  Created on: 17 janv. 2014
 *      Author: jonas
 */

#include "extra.h"
#include "../common/common.h"
#include "../common/text.h"

namespace sail
{

//void GnuplotExtra::setHue(double hue)
//{
//	uint8_t RGB[3];
//	hue2RGB(hue, RGB);
//
//}

GnuplotExtra::GnuplotExtra()
{
	//_rgbString = "";
}

void GnuplotExtra::plot(MDArray2d data)
{
	int rows = data.rows();
	if (data.cols() == 2)
	{
		plot_xy(data.sliceCol(0).getStorage().sliceTo(rows), data.sliceCol(1).getStorage().sliceTo(rows));
	}
	else if (data.cols() == 3)
	{
		plot_xyz(data.sliceCol(0).getStorage().sliceTo(rows), data.sliceCol(1).getStorage().sliceTo(rows), data.sliceCol(2).getStorage().sliceTo(rows));
	}
	else
	{
		cerr << "BAD NUMBER OF COLUMNS" << endl;
		throw std::exception();
	}
}

void GnuplotExtra::show()
{
	showonscreen();
	sleepForever();
}


} /* namespace sail */
