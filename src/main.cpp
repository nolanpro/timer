#include <Arduino.h>
#include <SPI.h>
#include <RF24.h> // NRF24L01+


///////////
// master has greentooth / old arduino / uup2
bool master = false;
///////////

// Modes
// 5 = calibarte
// 6 = watching for laser trigger
// 7 = trigger sent
int mode = 0;

// Signals
// 2 = start listening for trigger (move to mode 6 from 5 or 7)
// 3 = tigger!
// 4 = reset (go back to calibrate mode 0)
struct dataStruct{
  unsigned long time;
  int signal;
}data;

// Control Mode from bluetooth
// 0 = default
// 1 = run/reset
int controlMode = 0

int photocellPin = A1;
int photocellReading;
int LEDpin = 5;
int threshold = 500;
RF24 radio(9,10);
byte addresses[][6] = {"1Curl","2Curl"};
bool run = false;

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
  Serial.println("Now sending: " + String(thing.signal));
  if (!radio.write(&thing, sizeof(unsigned long)))
  {
    Serial.println("failed");
  }

}

void receive() {
  unsigned long started_waiting_at = micros();
  boolean timeout = false;

  while (!radio.available())
  {
    if (micros() - started_waiting_at > 200000)
    {
      timeout = true;
      break;
    }
  }

  if (timeout)
  {
    Serial.println("Failed, response timed out.");
  }
  else
  {
    radio.read(&data, sizeof(unsigned long));
  }
}

void trigger() {
  data.signal = 3;
  data.time = micros();
  send(data);
  mode = 7;
}

void runMasterRadio() {

  if (controlMode == 0 && data.signal != ????) {
    radio.stopListening();
    data.signal = 5;
    send(data);
  }
  if ()

  radio.stopListening();
  unsigned long start_time = micros();
  // Serial.println("Start_time is now " + String(start_time));
  // ledOn();

  data.time = start_time;
  data.signal = 2;
  send(data);
  radio.startListening();

  receive();
  unsigned long got_time = data.time;
  Serial.println("Got response " + String(got_time));

  unsigned long end_time = micros();

  // Serial.print(F(", Round-trip delay "));
  // Serial.print(end_time - start_time);
  // ledOff();
  // Serial.println(F(" microseconds"));

  // Serial.println("Waiting 1s");
  delay(1000);
}

void runSlaveRadio() {
  // Serial.println("STARTING SLAVE");
  if (radio.available()) {
    while (radio.available()) {
      radio.read(&data, sizeof(data));
    }
    if (data.signal == 2) {
      mode = 6;
    if (data.signal == 4)
      mode = 5;
    }
  }

  if (mode == 5) {
    calibrate();
  }

  if (mode == 6) {
    radio.stopListening();
    while(!isLaserOn()){ }
    trigger();
    radio.startListening();
  }

}

///////////////////////////////////////////////

void setup(void) {
  // debugging only
  Serial.begin(9600);

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
  radio.startListening();
}

void loop(void) {
  if (master) {
    runMasterRadio();
  } else {
    runSlaveRadio();
  }
  // delay(100);
}

