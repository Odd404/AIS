#ifndef AisMessageParser_h
#define AisMessageParser_h

#include <string>
#include <vector>

#include "AisMessage.h"
#include "AisDebug.h"

using namespace std;
/*
http://www.navcen.uscg.gov/
12. Why do I sometimes see more than one vessel with the same MMSI or vessel name (i.e. NAUT)? 
AIS users are required to operate their unit with a valid MMSI, unfortunately, 
some users neglect to do so (for example, use: 111111111, 123456789, 00000001, their U.S. documentation number, etc). 
A valid MMSI will start with a digit from 2 to 7, a U.S. assigned MMSI will start with either 338, 366, 367, 368, or 369. 
AIS users whom encounter a vessel using MMSI: 1193046 or named: NAUT should notify the user that their AIS unit is broadcasting improper data; 
see Nauticast AIS-MMSI Technical Bulletin for further information. All AIS users should check the accuracy of their AIS data prior to each voyage, 
and, particularly units that have been shutdown for any period of time. NOTE: If you are receiving (in range of) AIS reports from vessels using the same MMSI, 
they will appear as one vessel (jumping from position-to-position or line-to-line) on a graphical screen (e.g. ECS, ECDIS, radar) or on the AIS Minimal Keyboard Device (MKD).
*/

/**
Class that parses multiple (or single) AIS
sentences into an AisMessage object
*/
class AisMessageParser
{
private:
	std::string m_data;
	
public:
	AisMessageParser()
	{
		m_data.clear();
	}

	void clearData()
	{
		m_data.clear();
	}

	void addData(std::string data)
	{
		m_data.append(data);
	}

	AisMessage parseMessage()
	{
		// Convert AIS Message from 6-bit Hex to Binary
		vector<bool> aisBin;
		AisMessage aisMessage;

		try
		{
			AISMessageToBin(m_data, aisBin);
			DecodeAISMessage(aisBin, aisMessage);			
		}
		catch (...)
		{
            aisDebugError("parseMessage error");
		}		

		return aisMessage;
	}

private:
	int bin2dec(const vector<bool> &bin, int StartBit, int EndBit, bool isSigned = false)
	{
		int result = 0;

		if (isSigned)
		{
			bool FirstTrueFlag = false;
			vector<bool> binTC(EndBit - StartBit + 1, 0);

			// Determine if it's pos or neg
			if (bin[StartBit] == true)	//negative
			{
				// Find two's complement
				for (int p = EndBit, TCind = (binTC.size() - 1); p >= StartBit; p--, TCind--)
				{
					if (FirstTrueFlag)	// If we've already seen our first true, negate
					{
						binTC[TCind] = !(bin[p]);
					}
					else
					{
						if (bin[p])
						{
							FirstTrueFlag = true;
							binTC[TCind] = bin[p];
						}
						else
						{
							binTC[TCind] = bin[p];
						}
					}
				}	// End loop to find TC

				// Find the Result, then add negative
				for (unsigned int p = 0; p < binTC.size(); p++)
				{
					result = result * 2 + (int(binTC[p]));
				}

				result = -result;
			}
			else
			{
				for (int p = StartBit + 1; p <= EndBit; p++)
				{
					result = result * 2 + (int(bin[p]) - 0);
				}
			}	// End else (if not negative)
		}	// End if signed
		else
		{
			for (int p = StartBit; p <= EndBit; p++)
			{
				result = result * 2 + (int(bin[p]) - 0);
			}
		}
		
		return result;
	}

	int AISMessageToBin(string AISMessage, vector<bool> & AISBool)
	{
		int idxHex_Bin;
		vector<bool> Bin(6);
		int MaxVal = 63;
		int MinVal = 0;

		const char * tblHex_Bin[64] =
		{
			"000000", "000001", "000010", "000011",//1
			"000100", "000101", "000110", "000111",//2
			"001000", "001001", "001010", "001011",//3
			"001100", "001101", "001110", "001111",//4
			"010000", "010001", "010010", "010011",//5
			"010100", "010101", "010110", "010111",//6
			"011000", "011001", "011010", "011011",//7
			"011100", "011101", "011110", "011111",//8
			"100000", "100001", "100010", "100011",//9
			"100100", "100101", "100110", "100111",//10
			"101000", "101001", "101010", "101011",//11
			"101100", "101101", "101110", "101111",//12
			"110000", "110001", "110010", "110011",//13
			"110100", "110101", "110110", "110111",//14
			"111000", "111001", "111010", "111011",//15
			"111100", "111101", "111110", "111111" //16
		};

		for (int i = 0; i < AISMessage.length(); i++)
		{
			if (AISMessage[i] <= 87)
			{
				idxHex_Bin = AISMessage[i] - 48;
			}
			else
			{
				idxHex_Bin = AISMessage[i] - 56;
			}

			//????????
			if (idxHex_Bin < MinVal || idxHex_Bin > MaxVal)
			{
				break;
			}

			for (int j = 0; j < 6; j++)
			{
				if (tblHex_Bin[idxHex_Bin][j] == 48)
				{
					Bin[j] = false;
				}
				else
				{
					Bin[j] = true;
				}

				AISBool.push_back(Bin[j]);
			}
		}

		return 0;
	}

	int sgn(double in)
	{
		int out = 0;

		if (in < 0)
		{
			out = -1;
		}
		else if(in > 0)
		{
			out = 1;
		}
		else
		{
			out = 0;
		}

		return out;
	}

	/**
	Based on: http://gpsd.berlios.de/AIVDM.html
	*/
	int DecodeAISMessage(vector<bool> & AISBool, AisMessage &AISPosit)
	{
		if (AISBool.size() < 38)
		{
			return 0;
		}
		
		int MessageType = bin2dec(AISBool, 0, 5);
		bool debug = false;
		int PartNumber;
		double ROT = 0;

		if (AISBool.size() < 38)
		{
			return 0;
		}

		try
		{
			switch (MessageType)
			{
			case 1:		// Position Report with SOTDMA Position Report Class A
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setNAVSTATUS(bin2dec(AISBool, 38, 41));
					ROT = bin2dec(AISBool, 42, 49, true);
					AISPosit.setROT(sgn(ROT)*(ROT / 4.733)*(ROT / 4.733));
					AISPosit.setSOG((bin2dec(AISBool, 50, 59)) / 10.0);
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 60, 60));
					AISPosit.setLON(bin2dec(AISBool, 61, 88, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 89, 115, true) / 600000.0);
					AISPosit.setCOG((bin2dec(AISBool, 116, 127)) / 10.0);
					AISPosit.setTRUE_HEADING(bin2dec(AISBool, 128, 136));

					AISPosit.UTCTime = bin2dec(AISBool, 137, 142);
				}
				break;
			case 2:		// Position Report with SOTDMA Position Report Class A (Assigned schedule)
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setNAVSTATUS(bin2dec(AISBool, 38, 41));
					ROT = bin2dec(AISBool, 42, 49, true);
					AISPosit.setROT(sgn(ROT)*(ROT / 4.733)*(ROT / 4.733));
					AISPosit.setSOG((bin2dec(AISBool, 50, 59)) / 10.0);
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 60, 60));
					AISPosit.setLON(bin2dec(AISBool, 61, 88, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 89, 115, true) / 600000.0);
					AISPosit.setCOG((bin2dec(AISBool, 116, 127)) / 10.0);
					AISPosit.setTRUE_HEADING(bin2dec(AISBool, 128, 136));

					AISPosit.UTCTime = bin2dec(AISBool, 137, 142);
				}
				
				break;
			case 3:		// Position Report with ITDMA Position Report Class A (Response to interrogation)
				// Get MMSI (user ID)
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setNAVSTATUS(bin2dec(AISBool, 38, 41));
					ROT = bin2dec(AISBool, 42, 49, true);
					AISPosit.setROT(sgn(ROT)*(ROT / 4.733)*(ROT / 4.733));
					AISPosit.setSOG((bin2dec(AISBool, 50, 59)) / 10.0);
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 60, 60));
					AISPosit.setLON(bin2dec(AISBool, 61, 88, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 89, 115, true) / 600000.0);
					AISPosit.setCOG((bin2dec(AISBool, 116, 127)) / 10.0);
					AISPosit.setTRUE_HEADING(bin2dec(AISBool, 128, 136));

					AISPosit.UTCTime = bin2dec(AISBool, 137, 142);
				}
				break;
			case 4:		// Base Station Report
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.Year = bin2dec(AISBool, 38, 51);
					AISPosit.Month = bin2dec(AISBool, 52, 55);
					AISPosit.Day = bin2dec(AISBool, 56, 60);
					AISPosit.Hour = bin2dec(AISBool, 61, 65);
					AISPosit.Minute = bin2dec(AISBool, 66, 71);
					AISPosit.Second = bin2dec(AISBool, 72, 77);
					AISPosit.setLON(bin2dec(AISBool, 79, 106, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 107, 133, true) / 600000.0);
				}
				break;
			case 5:		// Ship Static and Voyage Related Data
				// Get MMSI (user ID)
				if (AISBool.size() == 424)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setIMO(bin2dec(AISBool, 40, 69));
					AISPosit.setCALLSIGN(bin2SixBitAISAscii(AISBool, 70, 111).c_str());
					AISPosit.setVESSELNAME(bin2SixBitAISAscii(AISBool, 112, 231).c_str());
					AISPosit.setVESSELTYPEINT(bin2dec(AISBool, 232, 239));
					AISPosit.setBOW(bin2dec(AISBool, 240, 248));
					AISPosit.setSTERN(bin2dec(AISBool, 249, 257));
					AISPosit.setPORT(bin2dec(AISBool, 258, 263));
					AISPosit.setSTARBOARD(bin2dec(AISBool, 264, 269));
					AISPosit.setETA(bin2dec(AISBool, 274, 293));
					AISPosit.setDRAUGHT(bin2dec(AISBool, 294, 301)*0.1);
					AISPosit.setDESTINATION(bin2SixBitAISAscii(AISBool, 302, 421).c_str());

					// Length is distance to Bow + distance to Stern
					AISPosit.setSHIPLENGTH(bin2dec(AISBool, 240, 248) + bin2dec(AISBool, 249, 257));
					// Width is Distance to Port + Distance to Starbord
					AISPosit.setSHIPWIDTH(bin2dec(AISBool, 258, 263) + bin2dec(AISBool, 264, 269));

					AISPosit.setPOSFIXTYPE(bin2dec(AISBool, 270, 273));
				}
				
				break;
			case 6:		// Addressed Binary Message
			case 7:		// Binary Acknowledgement
			case 8:		// Binary Broadcast Message
				AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
				AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
				break;
			case 9:		//Standard SAR Aircraft Position Report
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setLON(bin2dec(AISBool, 61, 88, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 89, 115, true) / 600000.0);
					AISPosit.setCOG(bin2dec(AISBool, 116, 127));
				}
				
				break;
			case 10:		// UTC and Date Inquiry
				AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
				AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
				break;
			case 11:		// UTC and Date Response
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.Year = bin2dec(AISBool, 38, 51);
					AISPosit.Month = bin2dec(AISBool, 52, 55);
					AISPosit.Day = bin2dec(AISBool, 56, 60);
					AISPosit.Hour = bin2dec(AISBool, 61, 65);
					AISPosit.Minute = bin2dec(AISBool, 66, 71);
					AISPosit.Second = bin2dec(AISBool, 72, 77);
					AISPosit.setLON(bin2dec(AISBool, 79, 106, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 107, 133, true) / 600000.0);
				}
				
				break;
			case 12:		// Addressed Safety Related Message
			case 13:		// Safety Related Acknowledge
			case 14:		// Safety Related Broadcast Message
				if (AISBool.size() >= 40 && AISBool.size() <= 1008)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setSAFETYMESSAGE(bin2SixBitAISAscii(AISBool, 40, AISBool.size() - 3).c_str());
				}
				
				break;
			case 15:		// Interrogation
			case 16:		// Assigned Mode Command
			case 17:		// GNSS Binary Broadcast Message
				if (AISBool.size() >= 80 && AISBool.size() <= 816)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setLON(bin2dec(AISBool, 40, 57, true) / 600.0);
					AISPosit.setLAT(bin2dec(AISBool, 58, 74, true) / 600.0);					
				}
				
				break;
			case 18:		// Standard Class B CS Position Report
				if (AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setSOG((bin2dec(AISBool, 46, 55)) / 10.0);
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 56, 56));
					AISPosit.setLON(bin2dec(AISBool, 57, 84, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 85, 111, true) / 600000.0);
					AISPosit.setCOG((bin2dec(AISBool, 112, 123)) / 10.0);
					AISPosit.setTRUE_HEADING(bin2dec(AISBool, 124, 132));

					AISPosit.UTCTime = bin2dec(AISBool, 133, 138);
				}

				break;
			case 19:		// Extended Class B Equipment Position Report
				if (AISBool.size() == 312)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setSOG((bin2dec(AISBool, 46, 55)) / 10.0);
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 56, 56));
					AISPosit.setLON(bin2dec(AISBool, 57, 84, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 85, 111, true) / 600000.0);
					AISPosit.setCOG((bin2dec(AISBool, 112, 123)) / 10.0);
					AISPosit.setTRUE_HEADING(bin2dec(AISBool, 124, 132));

					AISPosit.UTCTime = bin2dec(AISBool, 133, 138);

					AISPosit.setVESSELNAME(bin2SixBitAISAscii(AISBool, 143, 262).c_str());
					AISPosit.setVESSELTYPEINT(bin2dec(AISBool, 263, 270));
					AISPosit.setBOW(bin2dec(AISBool, 271, 279));
					AISPosit.setSTERN(bin2dec(AISBool, 280, 288));
					AISPosit.setPORT(bin2dec(AISBool, 289, 294));
					AISPosit.setSTARBOARD(bin2dec(AISBool, 295, 300));

					// Length is distance to Bow + distance to Stern
					AISPosit.setSHIPLENGTH(bin2dec(AISBool, 271, 279) + bin2dec(AISBool, 280, 288));
					// Width is Distance to Port + Distance to Starbord
					AISPosit.setSHIPWIDTH(bin2dec(AISBool, 289, 294) + bin2dec(AISBool, 295, 300));

					AISPosit.setPOSFIXTYPE(bin2dec(AISBool, 301, 304));
				}
				
				break;
			case 20:		// Data Link Management
				AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
				AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
				break;
			case 21:		// Aid-to-Navigation Report
				if (AISBool.size() >= 272 && AISBool.size() <= 360)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 163, 163));
					AISPosit.setLON(bin2dec(AISBool, 164, 191, true) / 600000.0);
					AISPosit.setLAT(bin2dec(AISBool, 192, 218, true) / 600000.0);
					AISPosit.setBOW(bin2dec(AISBool, 219, 227));
					AISPosit.setSTERN(bin2dec(AISBool, 228, 236));
					AISPosit.setPORT(bin2dec(AISBool, 237, 242));
					AISPosit.setSTARBOARD(bin2dec(AISBool, 243, 248));
				}
				
				break;
			case 22:		// Channel Management
			case 23:		// Group Assignment Command
				AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
				AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
				break;
			case 24:
				// Class B CS Static Data Report
				// There are two types
				if (AISBool.size() == 160 || AISBool.size() == 168)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					PartNumber = bin2dec(AISBool, 38, 39);
					AISPosit.setPARTNUMBER(PartNumber);

					if (PartNumber == 0)
					{
						AISPosit.setVESSELNAME(bin2SixBitAISAscii(AISBool, 40, 159).c_str());
					}
					else if (PartNumber == 1)
					{
						AISPosit.setVESSELTYPEINT(bin2dec(AISBool, 40, 47));
						AISPosit.setCALLSIGN(bin2SixBitAISAscii(AISBool, 90, 131).c_str());

						AISPosit.setBOW(bin2dec(AISBool, 132, 140));
						AISPosit.setSTERN(bin2dec(AISBool, 141, 149));
						AISPosit.setPORT(bin2dec(AISBool, 150, 155));
						AISPosit.setSTARBOARD(bin2dec(AISBool, 156, 161));

						// Length is distance to Bow + distance to Stern
						AISPosit.setSHIPLENGTH(bin2dec(AISBool, 132, 140) + bin2dec(AISBool, 141, 149));
						// Width is Distance to Port + Distance to Starbord
						AISPosit.setSHIPWIDTH(bin2dec(AISBool, 150, 155) + bin2dec(AISBool, 156, 161));
					}
					else
					{
						//PartNumber of 2 or 3 is not allowed
					}
				}
				break;
			case 25:		// Binary Message, Single Slot
			case 26:		// Binary Message, Multiple Slot
				AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
				AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
				break;
			case 27:		//Long Range AIS Broadcast message
				if (AISBool.size() == 96)
				{
					AISPosit.setMESSAGETYPE(bin2dec(AISBool, 0, 5));
					AISPosit.setMMSI(bin2dec(AISBool, 8, 37));
					AISPosit.setPOSACCURACY(bin2dec(AISBool, 38, 38));
					AISPosit.setNAVSTATUS(bin2dec(AISBool, 40, 43));
					AISPosit.setLON(bin2dec(AISBool, 44, 61, true) / 600.0);
					AISPosit.setLAT(bin2dec(AISBool, 62, 78, true) / 600.0);
					AISPosit.setSOG((bin2dec(AISBool, 79, 84)));
					AISPosit.setCOG((bin2dec(AISBool, 85, 93)));
				}

				break;
			default:
				aisDebugError("The message type was not recognized and therefore could not be decoded. MessageType=" << MessageType);
			}

			if (AISPosit.getLON() != -999)
			{
				wrapLonTo180(AISPosit);
			}
		}
		catch (out_of_range & e)
		{
			//?? vector ?? string ?? at ????????????????????????????
			//????????????????????????????
			cerr << e.what() << endl;
			getchar();
		}
		catch (...)
		{
			cout << "??????????2";
		}

		return 0;
	}

	void wrapLonTo180(AisMessage& m)
	{
		if (m.getLON() > 180.0)
		{
			do
			{
				m.setLON(m.getLON() - 360.0);
			} while ( m.getLON() > 180.0 );
		}
		else if (m.getLON() < -180.0)
		{
			do
			{
				m.setLON(m.getLON() + 360.0);
			} while (m.getLON() < -180.0);
		}

		return;
	}

	std::string bin2SixBitAISAscii(vector<bool> & AISBool, int StartInd, int EndInd)
	{
		// Determine How many 6 bit Ascii characters we have.
		int length = EndInd-StartInd + 1;
		int NumberChars = length/6;

		// Got through each 6-bit character, find ASCII
		std::string Output(NumberChars,'X');

		int DecAscii = 0;
		int z = StartInd;

		for(int x = 0; x < NumberChars; x++)     //reading in bytes. total characters = length / 6
		{
			if (AISBool.size() < z + 5)
			{
				break;
			}
			
			DecAscii = bin2dec(AISBool, z, z + 5);
			z += 6;

			if(DecAscii >= 0 && DecAscii <= 31)
			{
				Output[x] = (char)(DecAscii + 64);
			}
			else if(DecAscii >= 32 && DecAscii <= 63)
			{
				Output[x] = (char)DecAscii;
			}
			else
			{
				aisDebugError( "Unrecognized ascii character");
			}
		}

		//The string fields of BSVDM messages are padded with 0s
		//!BSVDM messages are padded with 0 == > corresponding to @
		//!AIVDM messages are padded with 32 == > corresponding to space
		//boost::trim_right_if(Output,boost::is_any_of("@"));
		//boost::erase_all(Output, "@");

		return Output;
	}
};

#endif