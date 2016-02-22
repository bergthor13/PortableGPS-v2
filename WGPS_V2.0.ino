#include <SPI.h>
#include <SD.h>

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
	dataFile.print(".000Z;");
	dataFile.print(latitude, 7);
	dataFile.print(";");
	dataFile.print(longitude, 7);
	dataFile.print(";");
	dataFile.print((gps.pvt.hMSL/1000.0), 3);
	dataFile.print(";");
	dataFile.print((gps.pvt.hAcc/1000.0), 3);
	dataFile.print(";");
	dataFile.print(voltageSmooth.average(), 3);

	if (dataFile.println() == 0) error(4);

	dataFile.flush();
}

// blink out an error code
void error(uint8_t errorNumber) {
	hadCard = false;
	digitalWrite(ledpinR, HIGH);
	digitalWrite(ledpinG, HIGH);
	digitalWrite(ledpinB, HIGH);
	while (true) {
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
			// Open the same file again.
			dataFile = openNewFile();
			hadCard = true;
			// Return to normal operation.
			return;
		}
		SD.end();
	}

}

void checkCard() {
	bool cardPresent = SD.exists(currFile);
	if (!cardPresent && hadCard) {
		Serial.println("Card removed");
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

void loop()
{
	checkCard();

	int type = gps.read();
	if (type == 2) {
		if (gps.statusOK()) {

			float measuredvbat = analogRead(A7);
			measuredvbat *= 2;    // we divided by 2, so multiply back
			measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
			measuredvbat /= 1024; // convert to voltage
			voltageSmooth.insert(measuredvbat);
			if (3.95 <= voltageSmooth.average()) {
				// GREEN
				digitalWrite(ledpinR, HIGH);
				digitalWrite(ledpinG, LOW);
				digitalWrite(ledpinB, HIGH);
			} else if (3.85 <= voltageSmooth.average() && voltageSmooth.average() < 3.95) {
				// GREEN and BLUE
				digitalWrite(ledpinR, HIGH);
				digitalWrite(ledpinG, LOW);
				digitalWrite(ledpinB, LOW);
			} else if (3.64 <= voltageSmooth.average() && voltageSmooth.average() < 3.85) {
				// BLUE
				digitalWrite(ledpinR, HIGH);
				digitalWrite(ledpinG, HIGH);
				digitalWrite(ledpinB, LOW);
			} else {
				// RED
				digitalWrite(ledpinR, LOW);
				digitalWrite(ledpinG, HIGH);
				digitalWrite(ledpinB, HIGH);
			}

			logToFile();
			digitalWrite(ledpinR, HIGH);
			digitalWrite(ledpinG, HIGH);
			digitalWrite(ledpinB, HIGH);

		} else {
			if (!gps.statusOK()) {
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
				gps.read();
				checkCard();
			}
		}
	} else if (type == -1) {
		Serial.println("INVALID!");
	}
}
