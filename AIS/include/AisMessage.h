#ifndef AISMESSAGE_H
#define AISMESSAGE_H

#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace std;

class AisMessage
{
private:
	int MESSAGETYPE;
	int MMSI;
	int NAVSTATUS;
	double ROT;
	double SOG;
	double LAT;
	double LON;
	double COG;
	double TRUE_HEADING;
	int DATETIME;
	double IMO;
	char VESSELNAME[33];
	int VESSELTYPEINT;
	double SHIPLENGTH;
	double SHIPWIDTH;
	double BOW;
	double STERN;
	double PORT;
	double STARBOARD;
	double DRAUGHT;
	char DESTINATION[33];
	char CALLSIGN[16];
	double POSACCURACY;
	double ETA;
	double PARTNUMBER;
	double POSFIXTYPE;
	char STREAMID[17];
	char SAFETYMESSAGE[161];

public:
	double time;
	int Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;
	double UTCTime;
	//std::string VesselType;
	//std::string Status;
	//std::string AISChannel;
	int ImageIndex;
	//std::string AssociatedImageTime;
	//std::string AssociatedImageSensor;
	double AssociatedImageResolution;

	AisMessage()
	{
		MESSAGETYPE = -1;
		MMSI = -1;
		NAVSTATUS = -1;
		ROT = -1 ;
		SOG = -1;
		LAT = -999;
		LON = -999;
		COG = -1;
		TRUE_HEADING = -999;
		DATETIME = -1;
		IMO = -1;
		memset(VESSELNAME, 0, sizeof(VESSELNAME));
		VESSELTYPEINT = -1;
		SHIPLENGTH = -1;
		SHIPWIDTH = -1;
		BOW = -1;
		STERN = -1;
		PORT = -1;
		STARBOARD = -1;
		DRAUGHT = -1;
		memset(DESTINATION, 0, sizeof(DESTINATION));
		memset(CALLSIGN, 0, sizeof(CALLSIGN));
		POSACCURACY = -1;
		ETA = -1;
		PARTNUMBER = -1;
		POSFIXTYPE = -1;
		memset(STREAMID, 0, sizeof(STREAMID));
		memset(SAFETYMESSAGE, 0, sizeof(SAFETYMESSAGE));

		//Additional things from original AISDot
		time = -1;
		Year = -1;
		Month = -1;
		Day = -1;
		Hour = -1;
		Minute = -1;
		Second = -1;
		UTCTime = -1;
		//VesselType = "";
		//Status = "";
		//AISChannel = "";
		ImageIndex = -1;
		//AssociatedImageTime = "";
		//AssociatedImageSensor = "";
		AssociatedImageResolution = -1;
	}

//	~AisMessage()
//    {
//        VesselType.~string();
//        Status.~string();
//        AISChannel.~string();
//        AssociatedImageSensor.~string();
//        AssociatedImageTime.~string();
//    }

	bool operator != (const AisMessage& lhs) const
	{
		return !(operator == (lhs));
	}

	bool operator == (const AisMessage& lhs) const
	{
		if(equalWithExceptionOfTime(lhs) && this->DATETIME == lhs.getDATETIME())
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool equalWithExceptionOfTime (const AisMessage& lhs) const
	{
		if(
			this->BOW == lhs.getBOW() &&
			(memcmp(this->CALLSIGN, lhs.getCALLSIGN(), sizeof(CALLSIGN)) == 0) &&
			this->COG == lhs.getCOG() &&
			(memcmp(this->DESTINATION, lhs.getDESTINATION(), sizeof(DESTINATION)) == 0) &&
			this->DRAUGHT == lhs.getDRAUGHT() &&
			this->ETA == lhs.getETA() &&
			this->PARTNUMBER == lhs.getPARTNUMBER() &&
			this->IMO == lhs.getIMO() &&
			this->LAT == lhs.getLAT() &&
			this->LON == lhs.getLON() &&
			this->MESSAGETYPE == lhs.getMESSAGETYPE() &&
			this->MMSI == lhs.getMMSI() &&
			this->NAVSTATUS == lhs.getNAVSTATUS() &&
			this->PORT == lhs.getPORT() &&
			this->POSACCURACY == lhs.getPOSACCURACY() &&
			this->POSFIXTYPE == lhs.getPOSFIXTYPE() &&
			this->ROT == lhs.getROT() &&
			this->SHIPLENGTH == lhs.getSHIPLENGTH() &&
			this->SHIPWIDTH == lhs.getSHIPWIDTH() &&
			this->SOG == lhs.getSOG() &&
			this->STARBOARD == lhs.getSTARBOARD() &&
			this->STERN == lhs.getSTERN() &&
			(memcmp(this->STREAMID, lhs.getSTREAMID(), sizeof(STREAMID)) == 0) &&
			this->TRUE_HEADING == lhs.getTRUE_HEADING() &&
			(memcmp(this->VESSELNAME, lhs.getVESSELNAME(), sizeof(VESSELNAME)) == 0) &&
			this->VESSELTYPEINT == lhs.getVESSELTYPEINT()&&
			memcmp(this->SAFETYMESSAGE, lhs.getSAFETYMESSAGE(), sizeof(SAFETYMESSAGE)) == 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	int getMMSI() const
	{
		return MMSI;
	}

	void setMMSI(int mmsi)
	{
		this->MMSI = mmsi;
	}

	int getNAVSTATUS() const
	{
		return NAVSTATUS;
	}

	void setNAVSTATUS(int navstatus)
	{
		this->NAVSTATUS = navstatus;
	}

	double getROT() const
	{
		return ROT;
	}

	void setROT(double rot)
	{
		this->ROT = rot;
	}

	double getSOG() const
	{
		return SOG;
	}

	void setSOG(double sog)
	{
		this->SOG = sog;
	}

	double getLAT() const
	{
		return LAT;
	}

	void setLAT(double lat)
	{
		this->LAT = lat;
	}

	double getLON() const
	{
		return LON;
	}

	void setLON(double lon)
	{
		this->LON = lon;
	}

	double getCOG() const
	{
		return COG;
	}

	void setCOG(double cog)
	{
		this->COG = cog;
	}

	double getTRUE_HEADING() const
	{
		return TRUE_HEADING;
	}

	void setTRUE_HEADING(double trueheading)
	{
		this->TRUE_HEADING = trueheading;
	}

	int getDATETIME() const
	{
		return DATETIME;
	}

	void setDATETIME(int datetime)
	{
		this->DATETIME = datetime;
	}

	int getMESSAGETYPE() const
	{
		return MESSAGETYPE;
	}

	void setMESSAGETYPE(int messagetype)
	{
		this->MESSAGETYPE = messagetype;
	}

	double getIMO() const
	{
		return IMO;
	}

	void setIMO(double imo)
	{
		this->IMO = imo;
	}

	const char* getVESSELNAME() const
	{
		return VESSELNAME;
	}

	void setVESSELNAME(const char *vesselname)
	{
		memcpy(this->VESSELNAME, vesselname, sizeof(VESSELNAME) - 1);
	}

	int getVESSELTYPEINT() const
	{
		return VESSELTYPEINT;
	}

	void setVESSELTYPEINT(int vesseltypeint)
	{
		this->VESSELTYPEINT = vesseltypeint;
	}

	void setSHIPLENGTH(double shiplength)
	{
		this->SHIPLENGTH = shiplength;
	}

	double getSHIPWIDTH() const
	{
		return SHIPWIDTH;
	}

	void setSHIPWIDTH(double shipwidth)
	{
		this->SHIPWIDTH = shipwidth;
	}

	double getBOW() const
	{
		return BOW;
	}

	/**
	* @return the SHIPLENGTH
	*/
	double getSHIPLENGTH() const
	{
		return SHIPLENGTH;
	}

	/**
	* @param BOW the BOW to set
	*/
	void setBOW(double BOW)
	{
		this->BOW = BOW;
	}

	/**
	* @return the STERN
	*/
	double getSTERN() const
	{
		return STERN;
	}

	/**
	* @param STERN the STERN to set
	*/
	void setSTERN(double STERN)
	{
		this->STERN = STERN;
	}

	/**
	* @return the PORT
	*/
	double getPORT() const
	{
		return PORT;
	}

	/**
	* @param PORT the PORT to set
	*/
	void setPORT(double PORT)
	{
		this->PORT = PORT;
	}

	/**
	* @return the STARBOARD
	*/
	double getSTARBOARD() const
	{
		return STARBOARD;
	}

	/**
	* @param STARBOARD the STARBOARD to set
	*/
	void setSTARBOARD(double STARBOARD)
	{
		this->STARBOARD = STARBOARD;
	}

	/**
	* @return the DRAUGHT
	*/
	double getDRAUGHT() const
	{
		return DRAUGHT;
	}

	/**
	* @param DRAUGHT the DRAUGHT to set
	*/
	void setDRAUGHT(double DRAUGHT)
	{
		this->DRAUGHT = DRAUGHT;
	}

	/**
	* @return the DESTINATION
	*/
	const char* getDESTINATION() const
	{
		return DESTINATION;
	}

	/**
	* @param DESTINATION the DESTINATION to set
	*/
	void setDESTINATION(const char* DESTINATION)
	{
		memcpy(this->DESTINATION, DESTINATION, sizeof(this->DESTINATION) - 1);
	}

	/**
	* @return the CALLSIGN
	*/
	const char* getCALLSIGN() const
	{
		return CALLSIGN;
	}

	/**
	* @param CALLSIGN the CALLSIGN to set
	*/
	void setCALLSIGN(const char* CALLSIGN)
	{
		memcpy(this->CALLSIGN, CALLSIGN, sizeof(this->CALLSIGN) - 1);
	}

	/**
	* @return the STREAMID
	*/
	const char* getSTREAMID() const
	{
		return STREAMID;
	}

	/**
	* @param STREAMID the STREAMID to set
	*/
	void setSTREAMID(const char* STREAMID)
	{
		memcpy(this->STREAMID, STREAMID, sizeof(this->STREAMID) - 1);
	}

	const char* getSAFETYMESSAGE() const
	{
		return SAFETYMESSAGE;
	}

	void setSAFETYMESSAGE(const char* safeMessage)
	{
		memcpy(this->SAFETYMESSAGE, safeMessage, sizeof(SAFETYMESSAGE) - 1);
	}

	/**
	* @return the POSACCURACY
	*/
	double getPOSACCURACY() const
	{
		return POSACCURACY;
	}

	/**
	* @param POSACCURACY the POSACCURACY to set
	*/
	void setPOSACCURACY(double POSACCURACY)
	{
		this->POSACCURACY = POSACCURACY;
	}

	/**
	* @return the ETA
	*/
	double getETA() const
	{
		return ETA;
	}

	/**
	* @param ETA the ETA to set
	*/
	void setETA(double ETA)
	{
		this->ETA = ETA;
	}

	/**
	* @return the PARTNUMBER
	*/
	double getPARTNUMBER() const
	{
		return PARTNUMBER;
	}

	/**
	* @param PARTNUMBER the PARTNUMBER to set
	*/
	void setPARTNUMBER(double PARTNUMBER)
	{
		this->PARTNUMBER = PARTNUMBER;
	}

	/**
	* @return the POSFIXTYPE
	*/
	double getPOSFIXTYPE() const
	{
		return POSFIXTYPE;
	}

	/**
	* @param POSFIXTYPE the POSFIXTYPE to set
	*/
	void setPOSFIXTYPE(double POSFIXTYPE)
	{
		this->POSFIXTYPE = POSFIXTYPE;
	}
};

#endif
