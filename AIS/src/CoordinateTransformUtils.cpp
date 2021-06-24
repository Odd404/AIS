#include "utils/CoordinateTransformUtils.h"

Coordinate CoordinateTransformUtils::bd09togcj02(double bd_lon, double bd_lat)
{
	Coordinate coordinate = { 0.0,0.0 };
	double x = bd_lon - 0.0065;
	double y = bd_lat - 0.006;
	double z = sqrt(x*x + y * y) - 0.00002*sin(y*x_PI);

	double theta = atan2(y, x) - 0.000003*cos(x*x_PI);

	double gg_lng = z * cos(theta);
	double gg_lat = z * sin(theta);

	coordinate.lat = gg_lat;
	coordinate.lng = gg_lng;

	return coordinate;
}

Coordinate CoordinateTransformUtils::gcj02tobd09(double lng, double lat)
{
	Coordinate coordinate = { 0.0,0.0 };

	double z = sqrt(lng*lng + lat * lat) + 0.00002*sin(lat*x_PI);
	double theta = atan2(lat, lng) + 0.000003*cos(lng*x_PI);

	double bd_lng = z * cos(theta) + 0.0065;
	double bd_lat = z * sin(theta) + 0.006;

	coordinate.lat = bd_lat;
	coordinate.lng = bd_lng;

	return coordinate;
}

Coordinate CoordinateTransformUtils::wgs84togcj02(double lng, double lat)
{
	Coordinate coordinate = { 0.0,0.0 };

	double dlat = transformlat(lng - 105.0, lat - 35.0);
	double dlng = transformlng(lng - 105.0, lat - 35.0);

	double radlat = lat / 180.0 * PI;
	double magic = sin(radlat);

	magic = 1 - ee * magic * magic;

	double sqrtmagic = sqrt(magic);

	dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * PI);
	dlng = (dlng * 180.0) / (a / sqrtmagic * cos(radlat) * PI);

	double mglat = lat + dlat;
	double mglng = lng + dlng;

	coordinate.lat = mglat;
	coordinate.lng = mglng;

	return coordinate;
}

Coordinate CoordinateTransformUtils::gcj02towgs84(double lng, double lat)
{
	Coordinate coordinate = { 0.0,0.0 };

	double dlat = transformlat(lng - 105.0, lat - 35.0);
	double dlng = transformlng(lng - 105.0, lat - 35.0);

	double radlat = lat / 180.0 * PI;
	double magic = sin(radlat);

	magic = 1 - ee * magic * magic;

	double sqrtmagic = sqrt(magic);

	dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * PI);
	dlng = (dlng * 180.0) / (a / sqrtmagic * cos(radlat) * PI);

	double mglat = lat + dlat;
	double mglng = lng + dlng;

	coordinate.lat = mglat;
	coordinate.lng = mglng;

	return coordinate;
}

Coordinate CoordinateTransformUtils::wgs84todb09(double lng, double lat)
{
	Coordinate coordinate = { 0.0,0.0 };

	double dlat = transformlat(lng - 105.0, lat - 35.0);
	double dlng = transformlng(lng - 105.0, lat - 35.0);

	double radlat = lat / 180.0 * PI;
	double magic = sin(radlat);

	magic = 1 - ee * magic * magic;

	double sqrtmagic = sqrt(magic);

	dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * PI);
	dlng = (dlng * 180.0) / (a / sqrtmagic * cos(radlat) * PI);

	double mglat = lat + dlat;
	double mglng = lng + dlng;

	//第二次转换
	double z = sqrt(mglng * mglng + mglat * mglat) + 0.00002 * sin(mglat * x_PI);
	double theta = atan2(mglat, mglng) + 0.000003 * cos(mglng * x_PI);

	double bd_lng = z * cos(theta) + 0.0065;
	double bd_lat = z * sin(theta) + 0.006;

	coordinate.lat = bd_lat;
	coordinate.lng = bd_lng;

	return coordinate;
}

double CoordinateTransformUtils::RealDistance(double lng1, double lat1, double lng2, double lat2)
{
	double a;
	double b;
	double radLat1 = rad(lat1);
	double radLat2 = rad(lat2);

	a = radLat1 - radLat2;
	b = rad(lng1) - rad(lng2);
	
	double s = 2 * asin(sqrt(pow(sin(a / 2), 2) + cos(radLat1) * cos(radLat2) * pow(sin(b / 2), 2)));
	
	s = s * EARTH_RADIUS;
	s = round(s * 10000) / 10000;

	return s;
}

double CoordinateTransformUtils::transformlat(double lng, double lat)
{
	double ret = -100.0 + 2.0 * lng + 3.0 * lat + 0.2 * lat * lat + 0.1 * lng * lat + 0.2 * sqrt(abs(lng));

	ret += (20.0 * sin(6.0 * lng * PI) + 20.0 * sin(2.0 * lng * PI)) * 2.0 / 3.0;
	ret += (20.0 * sin(lat * PI) + 40.0 * sin(lat / 3.0 * PI)) * 2.0 / 3.0;
	ret += (160.0 * sin(lat / 12.0 * PI) + 320 * sin(lat * PI / 30.0)) * 2.0 / 3.0;

	return ret;
}

double CoordinateTransformUtils::transformlng(double lng, double lat)
{
	double ret = 300.0 + lng + 2.0 * lat + 0.1 * lng * lng + 0.1 * lng * lat + 0.1 * sqrt(abs(lng));

	ret += (20.0 * sin(6.0 * lng * PI) + 20.0 * sin(2.0 * lng * PI)) * 2.0 / 3.0;
	ret += (20.0 * sin(lng * PI) + 40.0 * sin(lng / 3.0 * PI)) * 2.0 / 3.0;
	ret += (150.0 * sin(lng / 12.0 * PI) + 300.0 * sin(lng / 30.0 * PI)) * 2.0 / 3.0;

	return ret;
}

double CoordinateTransformUtils::rad(double d)
{
	return d * PI / 180.0;
}