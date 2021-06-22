// 坐标系转换类
#ifndef COORDINATETRANSFORMUTILS_H
#define COORDINATETRANSFORMUTILS_H
#pragma once
#include <string>
#include <math.h>

#define EARTH_RADIUS 6378137.0

struct Coordinate
{
	double lng;
	double lat;
};

class CoordinateTransformUtils
{
public:
	Coordinate bd09togcj02(double bd_lon, double bd_lat);
	Coordinate gcj02tobd09(double lng, double lat);
	Coordinate wgs84togcj02(double lng, double lat);
	Coordinate gcj02towgs84(double lng, double lat);
	Coordinate wgs84todb09(double lng, double lat);
	double RealDistance(double lng1, double lat1, double lng2, double lat2);
	double rad(double d);

private:
	double PI = 3.1415926535897932384626433832795;
	double x_PI = 3.14159265358979324 * 3000.0 / 180.0;
	double a = 6378245.0;
	double ee = 0.00669342162296594323;

	double transformlat(double lng, double lat);
	double transformlng(double lng, double lat);
};

#endif // !COORDINATETRANSFORMUTILS_H
