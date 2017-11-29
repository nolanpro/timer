#include <Arduino.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <RF24.h> // NRF24L01+


///////////
// master has greentooth / old arduino / uup2
bool master = false;
///////////

// Modes
// 0 = default state / do nothing
// 5 = calibarte
// 6 = watching for laser trigger
// 7 = trigger recieved!
// 8 = timeout from slave
int mode = 0;

// struct for sending data between arduinos
struct dataStruct{
  unsigned long time;
  int mode;
}data;

// Control Mode from bluetooth
// 0 = default
// 1 = run/reset
int controlMode = 0;

SoftwareSerial BTSerial(2, 3); // RX, TX
int photocellPin = A1;
int photocellReading;
int LEDpin = 5;
int threshold = 500;
RF24 radio(9,10);
byte addresses[][6] = {"1Curl","2Curl"};
bool run = false;

unsigned long startTriggerTime = 0;
unsigned long endTriggerTime = 0;
unsigned long totalTime = 0;
      
unsigned long startTimeout = 0;
unsigned long checkTime = 0;

bool isLaserOn() {
  photocellReading = analogRead(photocellPin);  
  if (photocellReading > threshold) {
    return true;
  } else {
    return false;
  }
}

void ledOn() {
  digitalWrite(LEDpin, HIGH);
}

void ledOff() {
  digitalWrite(LEDpin, LOW);
}

void flashOnce() {
  ledOn();
  delay(100);
  ledOff();
  delay(100);
}

void flash() {
  for (int i = 0; i < 5; i++) {
    flashOnce();
  }
}

void calibrate() {
  if (isLaserOn()) {
    ledOn();
  } else {
    ledOff();
  }
}

void send(dataStruct thing) {
  Serial.println("Now sending mode: " + String(thing.mode));
  if (!radio.write(&thing, sizeof(thing)))
  {
    Serial.println("failed");
  }
}

// void receive() {
//   unsigned long started_waiting_at = micros();
//   boolean timeout = false;

//   while (!radio.available())
//   {
//     if (micros() - started_waiting_at > 200000)
//     {
//       timeout = true;
//       break;
//     }
//   }

//   if (timeout)
//   {
//     Serial.println("Failed, response timed out.");
//   }
//   else
//   {
//     radio.read(&data, sizeof(unsigned long));
//   }
// }

void waitForFirstTrigger() {
  if (radio.available()) {
    while(radio.available()) {
      radio.read(&data, sizeof(data));
    }
    if (data.mode == 7) {
      startTriggerTime = micros();
      BTSerial.println("FirstTrigger");
      Serial.println("MASTER GOT MODE FROM SLAVE RADIO" + String(data.mode));
    }
    if (data.mode == 8) {
      Serial.println("Error: recieved timeout from slave");
    }
  }
}

void waitForSecondTrigger() {
  // BLOCKING
  bool timeout = false;
  while(isLaserOn()) {
    checkTime = micros();
    if (checkTime - startTriggerTime > 6000000) { // 6 seconds
      timeout = true;
      Serial.println("ERROR: Master timed out after 6 seconds");
      break;
    }
  }
  if (!timeout) {
    Serial.println("DONE!!! Master got laser trigger!");
    endTriggerTime = checkTime;
  }
}

void resetTimeValues() {
  startTriggerTime = 0;
  endTriggerTime = 0;
  totalTime = 0;
}

void runMasterRadio() {
  if (BTSerial.available()) {
    Serial.println("Got BT signal");
    char val = BTSerial.read();
    if (val == 'D') {
      controlMode = 0;
    }
    if (val == 'R') {
      controlMode = 1;
    }
  }
  
  if (controlMode == 0) {
    // set our mode to calibrate
    if (mode != 5) {
      mode = 5;
      // tell the other arduino to calibrate
      data.mode = 5;
      
      radio.stopListening();
      send(data);
      radio.startListening();
    }
  }

  if (controlMode == 1) {
    if (mode != 6 && !isLaserOn()) {
      Serial.println("ERROR: Laser is not on master. Back to mode 5");
      controlMode = 0;
      return;
    }

    // start watching for laser signal
    if (mode != 6) {
      mode = 6;
      // tell the other arduino to start watching also
      data.mode = 6;
      radio.stopListening();
      send(data);
      radio.startListening();

      ledOff();
      resetTimeValues();
      Serial.println("RESETTING TIME VALUES!!");
      startTimeout = micros();
      checkTime = 0;
    }


    if (mode == 6) {
      waitForFirstTrigger();
      if (startTriggerTime > 0) {
        Serial.println("Master is waiting for it's own laser trigger");
        waitForSecondTrigger();
        Serial.println("Done with 2nd trigger");

        if (endTriggerTime > 0) {
          totalTime = endTriggerTime - startTriggerTime;
        } else {
          Serial.println("endTriggerTime is 0, setting control mode to 0");
          controlMode = 0;
        }
      } else {
        checkTime = micros();
        if (checkTime - startTimeout > 35000000) {
          Serial.println("ERROR: Master timeout after 35 seconds, resetting to calibrate");
          mode = 5;
          data.mode = 5;
          controlMode = 0;
          radio.stopListening();
          send(data);
          radio.startListening();
        }
      }
    }

    if (totalTime > 0) {
      // we have a result!
      Serial.println("Sending result to BT: " + String(totalTime));
      BTSerial.println("RESULT:");
      BTSerial.println(totalTime);

      delay(1000);

      // LETS GO BACK TO CALIBRATE FOR NOW
      Serial.println("SENDING BOTH BACK TO CALIBRATE");
      mode = 5;
      data.mode = 5;
      controlMode = 0;
      radio.stopListening();
      send(data);
      radio.startListening();
    }

  }

  if (mode == 5) {
    calibrate();
  }

  // radio.stopListening();
  // unsigned long start_time = micros();
  // // Serial.println("Start_time is now " + String(start_time));
  // // ledOn();

  // data.time = start_time;
  // data.signal = 2;
  // send(data);
  // radio.startListening();
  // receive();
  // unsigned long got_time = data.time;
  // Serial.println("Got response " + String(got_time));

  // unsigned long end_time = micros();

  // Serial.print(F(", Round-trip delay "));
  // Serial.print(end_time - start_time);
  // ledOff();
  // Serial.println(F(" microseconds"));

  // Serial.println("Waiting 1s");
  // Serial.println("controlMode is: " + String(controlMode));
  // delay(1000);
}

void runSlaveRadio() {
  radio.startListening();
  if (radio.available()) {
    while (radio.available()) {
      radio.read(&data, sizeof(data));
    }
    if (data.mode == 5) {
      mode = 5;
      Serial.println("Received mode 5 from radio");
    }
    if (data.mode == 6) {
      mode = 6;
      Serial.println("Received mode 6 from radio");
    }
  }

  if (mode == 5) {
    calibrate();
    return;
  }

  if (mode == 6) {
    ledOff();
    if (!isLaserOn()) {
      Serial.println("ERROR: Laser is not on slave. Back to mode 5.");
      mode = 5;
      return;
    }

    radio.stopListening();
    unsigned long checkTime = 0;
    unsigned long startWatchingTime = 0;
    // bool gotTrigger = false;
    bool timeout = false;

    Serial.println("Slave is WATCHING LASER");
    startWatchingTime = micros();
    while(isLaserOn()) {
      checkTime = micros();
      if (checkTime - startWatchingTime > 30000000) { // 30 seconds
        timeout = true;
        break; // continue and check for new messages from radio
      }
    }
    if (!timeout && mode != 7) {
      mode = 7;
      data.mode = 7;
      send(data);
    }
    if (timeout) {
      Serial.println("ERROR: Slave timed out after 30 seconds");
      mode = 5;
      data.mode = 8;
      send(data);
    }
    radio.startListening();
  }
}

///////////////////////////////////////////////

void setup(void) {
  Serial.begin(9600);
  Serial.println("Welcome to USB Serial");

  if (master) {
    Serial.println("---MASTER---");
    BTSerial.begin(9600);
    BTSerial.println("Welcome to Bluetooth Serial");
  } else {
    Serial.println("---SLAVE---");
  }

  pinMode(photocellPin, INPUT);
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
  flash();

  // RADIO
  radio.begin();
  // radio.enableAckPayload();
  // radio.enableDynamicPayloads();
  if (master){
    radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1,addresses[0]);
  } else {
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1,addresses[1]);
  }
}

void loop(void) {
  if (master) {
    runMasterRadio();
  } else {
    runSlaveRadio();
  }
  // delay(100);
}

