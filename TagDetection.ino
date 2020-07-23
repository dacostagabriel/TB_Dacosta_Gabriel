#include <binary.h>
#include <SoftwareSerial.h>			  // Used for transmitting to the device
#include "SparkFun_UHF_RFID_Reader.h" // Library for controlling the M6E Nano module
#include <SPI.h>


SoftwareSerial softSerial(2, 3);	// RX, TX //softSerial(12, 13); //RX, TX 
SoftwareSerial hc06(12, 13);		// hc06(53, 52);


#define NB_MAX_TAGS 20			    // max 200 tags avec le code du 09.07.2020
#define SIZE_NUMBER_TAG 12			// number of byte in the tag number
#define TIME_TO_STOP_SCAN_MIN 1   // time in minutes


class Tag {
	byte tagEPCBytes; //Get the number of bytes of EPC from response

public:
	
	byte numberTag[SIZE_NUMBER_TAG];


/*  
	METHOD setEPCByte : set the EPU byte value and check if it's the same as the reference SIZE_NUMBER_TAG
	-------------------
	Input parameter :
	- byte valeByte : value receive to initialise the number of byte in EPU 
	-------------------
	return :
	- Nothing
*/
	void setEPCByte(byte valueByte) {
		if (SIZE_NUMBER_TAG != valueByte)
			Serial.println("ERROR, wrong size EPU byte, change the EPC paramter");
		else
			tagEPCBytes = valueByte;
	}


/*  
	METHOD returnEPC : return the number of byte in the EPC 
	-------------------
	Input parameter :
	- Nothing
	-------------------
	return :
	- byte tagEPCBytes : return the tagEPCBytes value
*/
	byte returnEPC() {
		return tagEPCBytes;
	}
};


class ListeTag {
private:
	Tag tagFound[NB_MAX_TAGS];		// all tags found
	short nbTag;					// number of tags found

public:
	ListeTag() :nbTag(0) {			// initialise the number of tag to 0
	}


/*  
	METHOD sameTag : check if the tag in parameter is not already in the list
	-------------------
	Input parameter :
	- Tag newTag : receive the new tag to check
	-------------------
	return :
	- bool : return true if the new tag is not in the list
*/
	bool sameTag(Tag tagTest) {
		int numberEqual(0);
		for (int i = 0; i < nbTag; i++) {
			for (int j = 6; j < tagFound[nbTag - 1].returnEPC(); j++)
				if (tagTest.numberTag[j] == tagFound[i].numberTag[j])
					numberEqual++;

			if (numberEqual >= tagFound[nbTag - 1].returnEPC() / 2)
				return(true);
			else
				numberEqual = 0;
		}
		return (false);
	}


/*  
	METHOD pushBack : add a new tag to the list
	-------------------
	Input parameter :
	- Tag newTag : receive the new tag to add
	-------------------
	return :
	- Nothing
*/
	void pushBack(Tag newTag) {
		tagFound[nbTag] = newTag;
		nbTag++;
	}
	

/*  
	METHOD printLastTag : serial print the last tag added to the list 
	-------------------
	Input parameter :
	- Nothing
	-------------------
	return :
	- Nothing
*/
	void printLastTag(void) {
		Serial.print("Tag numero : ");
		Serial.print(nbTag);

		//Print EPC bytes, this is a subsection of bytes from the response/msg array
		Serial.print(F(" epc["));
		for (byte x = 0; x < tagFound[nbTag - 1].returnEPC(); x++)
		{
			if (tagFound[nbTag - 1].numberTag[x] < 0x10)
				Serial.print(F("0"));
			Serial.print(tagFound[nbTag - 1].numberTag[x], HEX);   // obliger de faire tag -1 
			Serial.print(F(" "));
		}
		Serial.print(F("]"));
		Serial.println();
	}


/*  
	METHOD numberTag : return the number of tags in the list
	-------------------
	Input parameter :
	- Nothing
	-------------------
	return :
	-Tag nbTag : return the number of tags in the list
*/
	int numberTag(void) {
		return nbTag;
	}


/*  
	METHOD returnTag : return the choosen tag in the list
	-------------------
	Input parameter :
	-int selectedTag : select the tag to return with its position in the list 
	-------------------
	return :
	-Tag tagFound : return the choosen tag
*/
	Tag returnTag(int selectedTag)
	{
		if (selectedTag == 0) {
			Tag New;
			return New;	// if there is no tag in the list, it return a new tag
		}			
		return tagFound[selectedTag - 1];
	}
};


// declare global variables
ListeTag listeTag;
RFID nano;			 //Create instance




void setup()
{

	Serial.begin(115200);

	// Initialize Bluetooth module HC06
	hc06.begin(9600);

	//Wait for the serial port to come online
	while (!Serial);

	//Configure nano to run at 38400bps
	if (setupNano(38400) == false)
	{
		Serial.println(F("Module RFID failed to respond. Please check wiring."));
		while (1); //Freeze!
	}

	nano.setRegion(REGION_EUROPE);
	nano.setReadPower(2700); //5.00 dBm. Higher values may caues USB port to brown out
	//Max Read TX Power is 27.00 dBm and may cause temperature-limit throttling

	Serial.println("Initialize OK");


	// Print time scaning
	Serial.print("Scaning time : ");
	Serial.print(TIME_TO_STOP_SCAN_MIN);
	Serial.println(" [minute]");
	
	nano.startReading(); //Begin scanning for tags
}




void loop()
{
	
	if (nano.check() == true) //Check to see if any new data has come in from module
	{
		byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

		if (responseType == RESPONSE_IS_KEEPALIVE)
		{
			Serial.println("Scanning");
			Serial.print("Number of tags found : ");
			Serial.println(listeTag.numberTag());
		}
		else if (responseType == RESPONSE_IS_TAGFOUND)
		{
			Tag tagTest;
			tagTest.setEPCByte(nano.getTagEPCBytes()); //Get the number of bytes of EPC from response

			for (byte x = 0; x < tagTest.returnEPC(); x++)
				tagTest.numberTag[x] = nano.msg[x + 31];




			if (listeTag.sameTag(tagTest) == false) // Control if the tag found is a new one or not
			{
				listeTag.pushBack(tagTest);			// Add the tag to the list
				listeTag.printLastTag();			// print the new tag
				
			}
		}
		else if (responseType == ERROR_CORRUPT_RESPONSE)
		{
			Serial.println("Bad CRC");
		}
		else
		{
			//Unknown response
			Serial.print("Unknown error");
		}
	}


				/****************************************************/
				/*		SEND ALL TAGS NUMBER TO THE DATA BASE		*/
				/****************************************************/


	// wait until the choosen time, then send all the tags found by bluetooth
	if (millis() >= (TIME_TO_STOP_SCAN_MIN * 1000 * 60)) // time in minutes TIME_TO_STOP_SCAN_MIN / 1000/ 60
	{
		nano.stopReading();
		Serial.println("Stop tag scanning");

		bool allTagOk = false;
		unsigned long waitingTime;

		while (allTagOk == false)
		{
			for (int j = 1; j < listeTag.numberTag() + 1; j++)	// select all the tag one by one
			{
				Serial.print("Send tag number :");
				Serial.println(j);
				hc06.print("send");			// send the command "send" to begin the transmission
				for (int i = 0; i < SIZE_NUMBER_TAG; i++)	// select and send one byte at the time
				{
					// add zeros if the number has less than 3 digits 
					if ((int)listeTag.returnTag(j).numberTag[i] < (int)10 & (int)listeTag.returnTag(j).numberTag[i] != 0)
					{
						hc06.print("00");
						Serial.print("00");
					}
					else if ((int)listeTag.returnTag(j).numberTag[i] < (int)100 & (int)listeTag.returnTag(j).numberTag[i] != 0)
					{
						hc06.print("0");
						Serial.print("0");
					}
					if ((int)listeTag.returnTag(j).numberTag[i] == 0)
					{
						hc06.print("000");
						Serial.print("000");
					}
					else
					{
						Serial.print((int)listeTag.returnTag(j).numberTag[i]);
						hc06.print((int)listeTag.returnTag(j).numberTag[i]);
					}
					Serial.print(" ");
				}
				Serial.println("");
				hc06.print("end");  // message to validate the end the tag number
			}
			hc06.print("stop");   // send the command "stop" to stop the transmission
			
			

			// waiting for the acknowledge to confirm the succee of the transmission 
			waitingTime = millis();
			hc06.begin(9600);
			String ack = "";

			// after 2 secondes, the program assume than the transmission failed 
			while(((millis() - waitingTime) <= 200) && (ack.length() < 5))
				if (hc06.available())
					ack += (char)hc06.read();

			if (ack == "fine!")			// acknowledge OK, the tranmission was a succee
			{
				allTagOk = true;   
			}				 
			else if(ack.length() == 0)	// didn't have any acknowledge in 2 seconds, signal lost
			{
				allTagOk = false;
				Serial.println("Signal lost, restarting the transmission");
				hc06.print("fail");		// send the command to delete all the data and start again
			}
			else
			{
				allTagOk = false;		// didn't receive a good acknowledge, it will send again all the data	
				Serial.println("Wrong acknowledge, sending the data again");
			}
				allTagOk = false;		// didn't receive a good acknowledge, it will send again all the data	
		}			



		Serial.println("Transmission OK, freeze.");
		while(1);		// freeze
	}	
}


/*  Sparkfun function  */
//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
	nano.begin(softSerial); //Tell the library to communicate over software serial port

	//Test to see if we are already connected to a module
	//This would be the case if the Arduino has been reprogrammed and the module has stayed powered
	softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
	while (softSerial.isListening() == false); //Wait for port to open

	//About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
	while (softSerial.available()) softSerial.read();

	nano.getVersion();

	if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
	{
		//This happens if the baud rate is correct but the module is doing a ccontinuous read
		nano.stopReading();

		Serial.println(F("Module continuously reading. Asking it to stop..."));

		delay(1500);
	}
	else
	{
		//The module did not respond so assume it's just been powered on and communicating at 115200bps
		softSerial.begin(115200); //Start software serial at 115200

		nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

		softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate

		delay(250);
	}

	//Test the connection
	nano.getVersion();
	if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

	//The M6E has these settings no matter what
	nano.setTagProtocol(); //Set protocol to GEN2

	nano.setAntennaPort(); //Set TX/RX antenna ports to 1, there is only one antenna available

	return (true); 
}
