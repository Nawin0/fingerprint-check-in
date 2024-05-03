#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
SoftwareSerial mySerial(D6, D7);
#else

#define mySerial Serial1

#endif

#define I2CADDR 0x20
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '1', '4', '7', '*' },
  { '2', '5', '8', '0' },
  { '3', '6', '9', '#' },
  { 'A', 'B', 'C', 'D' }
};

byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6, 7 };

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574);
LiquidCrystal_I2C lcd(0x27, D2, D1);




Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
const char *ssid = "xxxx";
const char *password = "xxxxxx";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600);

// ################## Variables declaration ################################
#define MODE_NORMAL 0
#define MODE_ENROLL 1
#define MODE_DELETE 2
#define MODE_EMPTY 3
#define MODE_WAIT 4



uint8_t id;

unsigned char detected_id;
unsigned char working_mode;

bool flag_enrolling;
bool flag_deleting;

static String inputCode;


// #########################################################################
// ############################# SETUP #####################################
// #########################################################################
void setup() {

  Serial.begin(9600);
  Wire.begin();
  keypad.begin(makeKeymap(keys));
  lcd.begin(16, 2);
  lcd.backlight();

  while (!Serial)
    ;
  delay(100);
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");

  // Digital output pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // set the data rate for the sensor serial port
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    while (!finger.verifyPassword()) {
      Serial.println("Did not find fingerprint sensor :(");
      delay(1000);
      finger.begin(57600);
      delay(1000);
    }
  }
  connectToWiFi();

  getFingerprintID();
  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);

  finger.getTemplateCount();
  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data");
  } else {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }
  timeClient.begin();
}

// #########################################################################
// ############################# LOOP ######################################
// #########################################################################
void loop() {

  keypad_detection();
  fn_detection();
  fn_enrolling();
  fn_deleting();
  delay(50);
}

// #########################################################################
// #########################################################################
// ######################## FINGER PRINT ###################################
// #########################################################################
// #########################################################################

void keypad_detection() {
  char key = keypad.getKey();

  if (key) {
    uint8_t num;
    int Code = 0;
    int op2 = 0;

    if (working_mode == MODE_NORMAL) {
      if (key == 'C') {
        working_mode = MODE_WAIT;
        lcd.clear();
        Serial.println("Admin Mode");
        Serial.println(key);
        inputCode = "";

        lcd.setCursor(0, 0);
        lcd.print("Admin Mode");
        delay(3000);
        lcd.setCursor(0, 0);
        lcd.print("Entered pass: ");

      } else {
        Serial.println("\nCommand not matched\n");
      }
    } else {
      if (key == 'B') {
        working_mode = MODE_NORMAL;
        flag_enrolling = 0;
        flag_deleting = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Normal mode");
        delay(2000);
        lcd.clear();

        Serial.println("\nNormal mode\n");
      } else {
        switch (working_mode) {
          case MODE_ENROLL:
            if (key) {
              if (key == 'A') {
                int lastAIndex = inputCode.lastIndexOf('A');
                if (lastAIndex != -1) {
                  inputCode.remove(lastAIndex, 1);
                  lcd.clear();
                  lcd.setCursor(0, 1);
                  lcd.print(inputCode);
                }
              } else if (key == 'D') {
                if (inputCode.length() > 0) {
                  inputCode.remove(inputCode.length() - 1);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                  lcd.setCursor(0, 1);
                  lcd.print(inputCode);
                }
              } else {

                lcd.setCursor(0, 0);
                lcd.print("Entered ID:");
                lcd.setCursor(inputCode.length(), 1);
                lcd.print(key);
                inputCode += key;
              }

              if (key == 'A') {

                Serial.println("Entered ID: " + inputCode);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Entered ID:");
                lcd.setCursor(0, 1);
                lcd.print(inputCode);
                delay(1000);
                lcd.noBacklight();
                lcd.clear();
                lcd.backlight();

                op2 = inputCode.toInt();
                inputCode = "";
                if (op2 >= 1 && op2 <= 500) {
                  id = op2;
                  flag_enrolling = 1;
                  Serial.println("\nEnrolling ID #" + String(id) + "\n");

                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Enrolling ID #" + String(id));
                  sendToEnrollApi(id);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");

                } else {
                  Serial.println("\nInvalid ID. Please enter a value between 1 and 127.\n");
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                  lcd.setCursor(0, 1);
                  lcd.print("1 to 200");
                  delay(1000);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                }
              }
            }
            break;

          case MODE_DELETE:
            if (key) {
              if (key == 'A') {
                int lastAIndex = inputCode.lastIndexOf('A');
                if (lastAIndex != -1) {
                  inputCode.remove(lastAIndex, 1);
                  lcd.clear();
                  lcd.setCursor(0, 1);
                  lcd.print(inputCode);
                }
              } else if (key == 'D') {
                if (inputCode.length() > 0) {
                  inputCode.remove(inputCode.length() - 1);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                  lcd.setCursor(0, 1);
                  lcd.print(inputCode);
                }
              } else {

                lcd.setCursor(0, 0);
                lcd.print("Entered ID:");
                lcd.setCursor(inputCode.length(), 1);
                lcd.print(key);
                inputCode += key;
              }

              if (key == 'A') {

                Serial.println("Entered ID: " + inputCode);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Entered ID:");
                lcd.setCursor(0, 1);
                lcd.print(inputCode);
                delay(1000);
                lcd.noBacklight();
                lcd.clear();
                lcd.backlight();

                op2 = inputCode.toInt();
                inputCode = "";
                if (op2 >= 1 && op2 <= 127) {
                  id = op2;
                  flag_deleting = 1;
                  Serial.println("\nDeleting ID #" + String(id) + "\n");

                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Deleting ID #" + String(id));
                  sendToDeletingApi(id);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");

                } else {
                  Serial.println("\nInvalid ID. Please enter a value between 1 and 127.\n");
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                  lcd.setCursor(0, 1);
                  lcd.print("1 to 200");
                  delay(1000);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered ID:");
                }
              }
            }

            break;

          case MODE_WAIT:
            if (key >= '0' && key <= '9') {
              inputCode += key;
              lcd.setCursor(0, 0);
              lcd.print("Entered pass: ");

              lcd.setCursor(inputCode.length(), 1);
              lcd.print(key);

              if (inputCode.length() == 4) {

                Serial.print("Entered pass: " + inputCode);
                Code = inputCode.toInt();
                delay(2000);
                lcd.clear();
                if (Code == 1526) {
                  inputCode = "";
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("[1]Enroll Mode");
                  Serial.print("\n[1]Enroll Mode\n");
                  lcd.setCursor(0, 1);
                  lcd.print("[2]Delete Mode");
                  Serial.print("\n[2]Delete Mode\n");
                  while (true) {
                    char option = keypad.getKey();
                    if (option == '1') {
                      working_mode = MODE_ENROLL;
                      lcd.clear();
                      lcd.setCursor(0, 0);
                      lcd.print("Enroll Mode");
                      delay(1000);
                      lcd.clear();
                      lcd.setCursor(0, 0);
                      lcd.print("Entered ID:");


                      Serial.println("\nEnroll Mode");
                      Serial.println("Please type in the ID # (from 1 to 200) you want to save this finger as...");
                      Serial.println("Or type \"B\" to exit to normal operation\n");


                      break;
                    } else if (option == '2') {
                      working_mode = MODE_DELETE;
                      lcd.clear();
                      lcd.setCursor(0, 0);
                      lcd.print("Delete Mode");
                      delay(1000);
                      lcd.clear();
                      lcd.setCursor(0, 0);
                      lcd.print("Entered ID:");

                      Serial.println("\nDelete Mode");
                      Serial.println("Please type in the ID # (from 1 to 200) you want to delete...");
                      Serial.println("Or type \"B\" to exit to normal operation\n");
                      break;
                    }
                  }
                } else {
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Incorrect code");
                  delay(3000);
                  inputCode = "";
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Entered pass: ");
                }
              }
            }
        }
      }
    }
  }
}



/* ------------------------------------------------------------------------*/
void fn_enrolling() {
  if (flag_enrolling) {
    while (!getFingerprintEnroll())

      Serial.println();
    Serial.println("Waiting for a next finger....");
    Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
    Serial.println("Or type \"exit\" to exit to normal operation");
    Serial.println();
    flag_enrolling = 0;
  }
}

/* ------------------------------------------------------------------------*/
void sendToFingerprintApi(uint8_t id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Build the URL with query parameter
    String url = "xxxxxxxxxxx" + String(id);  // Use http for testing
    Serial.println("Sending request to URL: " + url);

    WiFiClient client;  // Use WiFiClient for testing without SSL
    http.begin(client, url);

    // Change HTTP method to POST
    int httpCode = http.POST("");  // Empty body for POST request
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        // Successful response, read the content
        String response = http.getString();
        Serial.println("ID sent to FastAPI successfully");
        Serial.print("Response: ");
        Serial.println(response);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        String user = doc["user"];
        String pin = doc["pin"];
        String timestamp = doc["timestamp"];

        Serial.println("user:" + user);
        Serial.println("pin:" + pin);
        Serial.println("time:" + timestamp);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Name:" + user);
        lcd.setCursor(0, 1);
        lcd.print("ID:" + pin);
        delay(2000);
        lcd.clear();

      } else {
        Serial.printf("Failed to send ID to FastAPI, HTTP error code: %d\n", httpCode);
      }
    } else {
      Serial.println("Failed to connect to FastAPI");
    }

    http.end();
  }
}


void sendToEnrollApi(uint8_t id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Build the URL with query parameter
    String url = "xxxxxxxxxxxx" + String(id);  // Use http for testing
    Serial.println("Sending request to URL: " + url);

    WiFiClient client;  // Use WiFiClient for testing without SSL
    http.begin(client, url);

    // Change HTTP method to POST
    int httpCode = http.POST("");  // Empty body for POST request
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);

    http.end();
  }
}

void sendToDeletingApi(uint8_t id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Build the URL with query parameter
    String url = "xxxxxxxxxxxxxs" + String(id);  // Use http for testing
    Serial.println("Sending request to URL: " + url);

    WiFiClient client;  // Use WiFiClient for testing without SSL
    http.begin(client, url);

    // Change HTTP method to POST
    int httpCode = http.POST("");  // Empty body for POST request
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);

    http.end();
  }
}


void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
  } else {
    Serial.println("\nFailed to conn  ect to WiFi. Please check your credentials or connection.");
  }
}



void fn_detection() {
  if (working_mode == MODE_NORMAL) {
    getFingerprintID();

    if (detected_id) {
      Serial.println();
      Serial.println("ID: " + String(detected_id) + " is detected");
      sendToFingerprintApi(detected_id);
      Serial.println();
      detected_id = 0;
    }
  }
}


/* ---------------------------------------------------------------------- */
void fn_deleting() {
  if (flag_deleting) {
    deleteFingerprint(id);
    Serial.println();
    Serial.println("Waiting for a next ID....");
    Serial.println("Please type in the ID # (from 1 to 127) you want to delete...");
    Serial.println("Or type \"exit\" to exit to normal operation");
    Serial.println();
    flag_deleting = 0;
  }
}


// #########################################################################
// #########################################################################
// ###################### FINGER PRINT DETECTION ###########################
// #########################################################################
// #########################################################################

unsigned long get_finger_ms;
unsigned long get_finger_time_buf;
unsigned long get_finger_time_dif;

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      timeClient.update();
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print(timeClient.getFormattedTime());

      get_finger_time_dif = millis() - get_finger_time_buf;
      if (get_finger_time_dif >= 3000)  // print messages ever 3 seconds
      {
        get_finger_time_buf = millis();
        Serial.println("No finger detected");

        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      }
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Not match");
    delay(1000);

    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  detected_id = finger.fingerID;
  return finger.fingerID;
}

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}

// #########################################################################
// #########################################################################
// ###################### FINGER PRINT ENROLLMENT ##########################
// #########################################################################
// #########################################################################

uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("finger to enroll");
  lcd.setCursor(0, 1);
  lcd.print(id);

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove finger");

  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID" + String(id));
  delay(1000);

  p = -1;
  Serial.println("Place same finger again");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("finger again");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");

    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("not matched");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Entered ID:");

    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);



  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");

    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ID" + String(id));


    lcd.setCursor(0, 1);
    lcd.print("Stored!");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Entered ID:");

  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

// #########################################################################
// #########################################################################
// ####################### FINGER PRINT DELETING ###########################
// #########################################################################
// #########################################################################
uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Deleted!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Entered ID:");


  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else if (p == FINGERPRINT_UPLOADFAIL) {
    Serial.println("Failed to upload template");
  } else {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }

  return p;
}