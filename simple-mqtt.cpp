/*******************************************************************************

  Bare Conductive Pi Cap
  ----------------------

  simple-mqtt.cpp - sends capacitive touch / release data from MPR121 to a
  specified MQTT broker.

  Written for Raspberry Pi.

  Original example by Sven Haiges.

  Bare Conductive code written by Szymon Kaliski.

  This work is licensed under a MIT license https://opensource.org/licenses/MIT

  Copyright (c) 2016, Bare Conductive

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

 *******************************************************************************/

#include <MPR121.h>
#include <signal.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <mosquitto.h>

#define NUM_ELECTRODES 12

using namespace std;

bool volatile keepRunning = true;

// this allows us to exit the program via Ctrl+C while still exiting elegantly
void intHandler(int dummy) {
  keepRunning = false;
  exit(0);
}

// print help
void printHelp() {
  cout
    << "Sends Pi Cap touch readings through MQTT - MUST be run as root." << endl << endl
    << "Usage: simple-mqtt [OPTION]" << endl << endl
    << "Options:" << endl
    << "  -b, --broker    MQTT broker [REQUIRED]" << endl
    << "  -u, --username  MQTT broker username [OPTIONAL]" << endl
    << "  -p, --password  MQTT broker password [OPTIONAL]" << endl
    << "      --help      displays this message" << endl;

  exit(0);
}

int main(int argc, char* argv[]) {
  string broker   = "";
  string username = "";
  string password = "";

  // sift through the arguments and set stuff up / show help as appropriate
  for (int i = 0; i < argc; i++) {
    if ((string)argv[i] == "-b" || (string)argv[i] == "--broker" ) {
      broker = (string)argv[i+1];
    }
    if ((string)argv[i] == "-u" || (string)argv[i] == "--username" ) {
      username = (string)argv[i+1];
    }
    if ((string)argv[i] == "-p" || (string)argv[i] == "--password" ) {
      password = (string)argv[i+1];
    }
    else if ((string)argv[i] == "--help") {
      printHelp();
    }
  }

  // stop if no broker is provided
  if (broker.length() == 0) {
    printHelp();
  }

  // register our interrupt handler for the Ctrl+C signal
  signal(SIGINT, intHandler);

  // start MQTT lib
  mosquitto_lib_init();
  struct mosquitto *client = mosquitto_new(NULL, true, NULL);

  // authenticate if username was provided
  bool hasUsername = username.length() > 0;
  bool hasPassword = password.length() > 0;

  if (hasUsername) {
    if (hasPassword) {
      // set password if password was provided
      mosquitto_username_pw_set(client, username.c_str(), password.c_str());
    }
    else {
      // otherwise authenticate with username only
      mosquitto_username_pw_set(client, username.c_str(), NULL);
    }
  }

  mosquitto_connect(client, broker.c_str(), 1883, 2);

  // default MPR121 address on the Pi Cap
  if (!MPR121.begin(0x5C)) {
    cout << "error setting up MPR121: ";

    switch (MPR121.getError()) {
      case NO_ERROR:
        cout << "no error" << endl;
        break;
      case ADDRESS_UNKNOWN:
        cout << "incorrect address" << endl;
        break;
      case READBACK_FAIL:
        cout << "readback failure" << endl;
        break;
      case OVERCURRENT_FLAG:
        cout << "overcurrent on REXT pin" << endl;
        break;
      case OUT_OF_RANGE:
        cout << "electrode out of range" << endl;
        break;
      case NOT_INITED:
        cout << "not initialised" << endl;
        break;
      default:
        cout << "unknown error" << endl;
        break;
    }

    exit(1);
  }

  // this is the touch threshold - setting it low makes it more like a proximity trigger
  // default value is 40 for touch
  int touchThreshold = 40;

  // this is the release threshold - must ALWAYS be smaller than the touch threshold
  // default value is 20 for touch
  int releaseThreshold = 20;

  MPR121.setTouchThreshold(touchThreshold);
  MPR121.setReleaseThreshold(releaseThreshold);

  while (keepRunning) {
    if (MPR121.touchStatusChanged()) {
      MPR121.updateTouchData();

      for (int i=0; i < NUM_ELECTRODES; i++) {
        // we need electrode number as char array
        char buffer[2];
        sprintf(buffer, "%d", i);

        if (MPR121.isNewTouch(i)) {
          if (hasUsername) {
            mosquitto_publish(client, NULL, (username + "/feeds/picap-touched").c_str(), 2, buffer, 0, false);
          }
          else {
            mosquitto_publish(client, NULL, "/feeds/picap-touched", 2, buffer, 0, false);
          }
        }
        else if (MPR121.isNewRelease(i)) {
          if (hasUsername) {
            mosquitto_publish(client, NULL, (username + "/feeds/picap-released").c_str(), 2, buffer, 0, false);
          }
          else {
            mosquitto_publish(client, NULL, "/feeds/picap-released", 2, buffer, 0, false);
          }
        }
      }
    }

    // run mosquitto network loop
    mosquitto_loop(client, 0, 1);

    // a little delay so that we don't just sit chewing CPU cycles
    // could implement this with proper interrupts for greater efficiency
    delay(10);
  }

  // make sure we return gracefully
  return(0);
}
