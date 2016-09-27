#include <SPI.h>
#include <SD.h>
#include "MAX17043.h"
#include "Wire.h"

#define ledpinR 11
#define ledpinG 12
#define ledpinB 10

#define SMOOTH_SIZE 120

const unsigned char UBX_HEADER[]      = { 0xB5, 0x62 };

const int chipSelect = 5;
const int cd = 6;
boolean buttonState = LOW;
const int buttonPin = 13;
int fileNumber = 0;
bool hadCard = false;
char currFile[15];

MAX17043 batteryMonitor;
File dataFile;
File openNewFile() {
  char filename[15];
  strcpy(filename, "LOG0000.csv");
  for (; fileNumber < 10000; fileNumber++) {
    filename[3] = '0' + (fileNumber/1000)%10;
    filename[4] = '0' + (fileNumber/100)%10;
    filename[5] = '0' + (fileNumber/10)%10;
    filename[6] = '0' + fileNumber%10;

    // create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }

  // Open the file to write and return it.
  for (int i=0; i<15; i++){
      currFile[i] = filename[i];
  }
  return SD.open(filename, FILE_WRITE);
}

byte waitForByte() {
	while (!Serial1.available());
	return Serial1.read();
}

struct Smoother {
  double data[SMOOTH_SIZE];
  int index;
  bool full;
  Smoother() {
    for (int i = 0; i < SMOOTH_SIZE; i++) {
      data[i] = NULL;
    }
    index = 0;
    full = false;
  }

  void insert(double dataPoint) {
    data[index] = dataPoint;
    if (index != SMOOTH_SIZE - 1) {
      index++;
    } else {
      index = 0;
      full = true;
    }
  }

  double average() {
    double sum = 0;
    for (int i = 0; i < SMOOTH_SIZE; i++) {
      if (data[i] != NULL) {
        sum += data[i];
      }
    }
    if (full) {
        return (double)sum/(double)SMOOTH_SIZE;
    } else {
        return (double)sum/(double)index;
    }
  }
  	double asdf = 4.2;
	double average2() {
		if (asdf < 3.5) asdf = 4.2;
		return asdf -=0.01;
	}
};
Smoother voltageSmooth;

class Ublox_GPS {

	public:
	struct UBX_NAV_PVT {
		unsigned char cls;
		unsigned char id;
		unsigned short len;
		unsigned long iTOW;
		unsigned short year;
		unsigned char month;
		unsigned char day;
		unsigned char hour;
		unsigned char min;
		unsigned char sec;
		unsigned char valid;
		unsigned long tAcc;
		signed long nano;
		unsigned char fixType;
		signed char flags;
		unsigned char reserved1;
		unsigned char numSv;
		signed long lon;
		signed long lat;
		signed long height;
		signed long hMSL;
		unsigned long hAcc;
		unsigned long vAcc;
		signed long velN;
		signed long velE;
		signed long velD;
		signed long gSpeed;
		signed long headMot;
		unsigned long sAcc;
		unsigned long headAcc;
		unsigned short pDop;
		unsigned char reserved20;
		unsigned char reserved21;
		unsigned char reserved22;
		unsigned char reserved23;
		unsigned char reserved24;
		unsigned char reserved25;
		unsigned long headVeh;
		unsigned char reserved30;
		unsigned char reserved31;
		unsigned char reserved32;
		unsigned char reserved33;
	};
	
	UBX_NAV_PVT pvt;
	bool calcChecksum(UBX_NAV_PVT pvtCheck, byte chkA, byte chkB) {
		unsigned char CK_A;
		unsigned char CK_B;
		memset(&CK_A, 0, 1);
		memset(&CK_B, 0, 1);

		for (int i = 0; i < (int)sizeof(pvtCheck); i++) {
			CK_A += ((unsigned char*)(&pvtCheck))[i];
			CK_B += CK_A;
		}
		
		if (chkA == CK_A && chkB == CK_B) return true;
		else return false;
	}
	bool statusOK() {
		return pvt.valid & 0x1 &&            // Valid Date
					 pvt.valid & 0x2 >> 1 && // Valid Time
					 pvt.valid & 0x4 >> 2 && // Fully Resolved
					 pvt.flags & 0x1;        // GNSS Fix OK
	}

	int read() {
		byte hdr1, hdr2;
		hdr1 = waitForByte();
		hdr2 = waitForByte();
		int asdf2 = 0;
		
		// Find the UBX header.
		while (true) {
			if (hdr1 == UBX_HEADER[0] && hdr2 == UBX_HEADER[1]) {
				break;
			} else {
				hdr1 = hdr2;
				hdr2 = waitForByte();
				continue;
			}
		}
		// Copy all data to the corresponding structure.
		for (int i=0; i < sizeof(pvt); i++){
			((unsigned char*)(&pvt))[i] = waitForByte();
		}
		
		if (calcChecksum(pvt, waitForByte(), waitForByte())) {
			return 2;
		} else {
			return -1;
		}
	}
};

Ublox_GPS gps;

void logToFile() {
	double latitude = gps.pvt.lat/10000000.0;
	double longitude = gps.pvt.lon/10000000.0;
	dataFile.print(gps.pvt.year);
	dataFile.print("-");
	if (gps.pvt.month < 10) dataFile.print("0");
	dataFile.print(gps.pvt.month);
	dataFile.print("-");
	if (gps.pvt.day < 10) dataFile.print("0");
	dataFile.print(gps.pvt.day);
	dataFile.print("T");
	if (gps.pvt.hour < 10) dataFile.print("0");
	dataFile.print(gps.pvt.hour);
	dataFile.print(":");
	if (gps.pvt.min < 10) dataFile.print("0");
	dataFile.print(gps.pvt.min);
	dataFile.print(":");
	if (gps.pvt.sec < 10) dataFile.print("0");
	dataFile.print(gps.pvt.sec);
	double nano = gps.pvt.nano;
  dataFile.print(".");
  if (round(nano/1000000.0) < 0){
    dataFile.print("000");
  } else {
      if (round(nano/1000000.0) < 10) dataFile.print("0");
      if (round(nano/1000000.0) < 100) dataFile.print("0");
      dataFile.print(round(nano/1000000.0));
  }
  dataFile.print("Z;");
	dataFile.print(latitude, 7);
	dataFile.print(";");
	dataFile.print(longitude, 7);
	dataFile.print(";");
	dataFile.print((gps.pvt.hMSL/1000.0), 3);
	dataFile.print(";");
	dataFile.print((gps.pvt.hAcc/1000.0), 3);
	dataFile.print(";");
	dataFile.print(batteryMonitor.getVCell(), 3);
  dataFile.print(";");
  dataFile.print(voltageSmooth.average(),3);
  dataFile.print(";");
  dataFile.print(batteryMonitor.getCompensateValue(), HEX);
  dataFile.print(";");
  dataFile.print(batteryMonitor.getSoC(), 2);

	if (dataFile.println() == 0) error(4);

	dataFile.flush();
}

// blink out an error code
void error(uint8_t errorNumber) {
	hadCard = false;
	digitalWrite(ledpinR, HIGH);
	digitalWrite(ledpinG, HIGH);
	digitalWrite(ledpinB, HIGH);
	SD.end();
	while (true) {
    voltageSmooth.insert(batteryRead());
		uint8_t i;
		for (i=0; i<errorNumber; i++) {
			digitalWrite(ledpinR, LOW);
			delay(200);
			digitalWrite(ledpinR, HIGH);
			delay(200);
		}
		for (i=errorNumber; i<10; i++) {
			delay(100);
		}
		// Check if a card has been inserted.
		if (SD.begin(chipSelect)) {
			// Open a new file again.
			dataFile = openNewFile();
			hadCard = true;
			// Return to normal operation.
			return;
		}
	}

}

void checkCard() {
	bool cardPresent = SD.exists(currFile);
	if (!cardPresent && hadCard) {
		Serial.println("Card removed");
		dataFile.flush();
		SD.end();
		hadCard = false;
		error(4);
	}

	if (cardPresent && !hadCard) {
		Serial.println("Card inserted");
		hadCard = true;
		if (SD.begin(chipSelect)) {
			dataFile = openNewFile();
		}
	}
}

void setup()
{
  Wire.begin();
	Serial.begin(115200);
	Serial.println("Welcome to my Ublox GPS Parser!");
	Serial1.begin(9600);

	pinMode(SS, OUTPUT);
	pinMode(cd, INPUT);
	pinMode(ledpinR, OUTPUT);
	pinMode(ledpinG, OUTPUT);
	pinMode(ledpinB, OUTPUT);
	pinMode(A7, INPUT);
	digitalWrite(ledpinR, HIGH);
	digitalWrite(ledpinG, HIGH);
	digitalWrite(ledpinB, HIGH);

	Serial.print("Initializing SD card...");
	// see if the card is present and can be initialized:
	if (!SD.begin(chipSelect)) {
		if (digitalRead(cd) == HIGH) {
			Serial.println("Card failed");
		} else {
			Serial.println("Card not present");
		}
		error(2);
	}
	Serial.println("card initialized.");

	dataFile = openNewFile();
	
	if (!dataFile) {
		Serial.println("error opening file");
		error(3);
	}
}
// A custom map function for float numbers.
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
float batteryRead() {
  float measuredvbat = analogRead(A7);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

void noSignalBlink() {
  digitalWrite(ledpinR, HIGH);
  digitalWrite(ledpinG, HIGH);
  digitalWrite(ledpinB, LOW);
  delay(100);
  digitalWrite(ledpinR, HIGH);
  digitalWrite(ledpinG, HIGH);
  digitalWrite(ledpinB, HIGH);
  delay(200);
  digitalWrite(ledpinR, HIGH);
  digitalWrite(ledpinG, HIGH);
  digitalWrite(ledpinB, LOW);
  delay(100);
  digitalWrite(ledpinR, HIGH);
  digitalWrite(ledpinG, HIGH);
  digitalWrite(ledpinB, HIGH);
}

void turnAllLedsOff() {
  digitalWrite(ledpinR, HIGH);
  digitalWrite(ledpinG, HIGH);
  digitalWrite(ledpinB, HIGH);
}

void batteryStatusColor(double value){
    if (99 <= value) {
    // WHITE
    analogWrite(ledpinR, 150);
    analogWrite(ledpinG, 100);
    analogWrite(ledpinB, 0);
  } else if (5 <= value && value < 99) {
    // GREEN -> BLUE
    int gVal = (int)mapfloat(value, 5, 100, 255, 0);
    int bVal = (int)mapfloat(value, 5, 100, 0, 255);
    digitalWrite(ledpinR, HIGH);
    analogWrite(ledpinG, gVal);
    analogWrite(ledpinB, bVal);
  
  } else if (1.5 <= value && value < 5) {
    // BLUE -> RED
    int bVal = (int)mapfloat(value, 1.5, 5, 255, 0);
    int rVal = (int)mapfloat(value, 1.5, 5, 100, 255);
    analogWrite(ledpinR,rVal);
    digitalWrite(ledpinG, HIGH);
    analogWrite(ledpinB, bVal);
  } else {
    // WHITE
    analogWrite(ledpinR, 150);
    analogWrite(ledpinG, 100);
    analogWrite(ledpinB, 0);
  }
}
double oldVoltage = -1;
void loop()
{
	int type = gps.read();
	if (type == 2) {
		if (gps.statusOK()) {
			voltageSmooth.insert(batteryRead());
     double smoothVoltage = batteryMonitor.getSoC();
     if (oldVoltage != smoothVoltage) {
       Serial.print(smoothVoltage);Serial.println("%");
       oldVoltage = smoothVoltage;
     }
     
      batteryStatusColor(smoothVoltage);
			checkCard();
			logToFile();
      turnAllLedsOff();

		} else {
			while (!gps.statusOK()) {
				checkCard();
        noSignalBlink();
				gps.read();
				delay(1000);
			}
		}
	} else if (type == -1) {
		Serial.println("INVALID!");
	}
}
