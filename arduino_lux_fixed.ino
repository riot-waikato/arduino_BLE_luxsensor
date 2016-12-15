#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include "lib_aci.h"
#include "aci_setup.h"

//############################################## - Ambient Light
#define TSL2572_I2CADDR     0x39

#define GAIN_1X 0
#define GAIN_8X 1
#define GAIN_16X 2
#define GAIN_120X 3

//only use this with 1x and 8x gain settings
#define GAIN_DIVIDE_6 true

//#define DEBUG
#define DEVICE "0x00"

int gain_val = 0;
//##############################################

#include "services.h"

unsigned long lastUpdate = millis();
bool notifyTemp = false;
bool broadcastSet = false;

#ifdef SERVICES_PIPE_TYPE_MAPPING_CONTENT
static services_pipe_type_mapping_t
services_pipe_type_mapping[NUMBER_OF_PIPES] =
  SERVICES_PIPE_TYPE_MAPPING_CONTENT;
#else
#define NUMBER_OF_PIPES 0
static services_pipe_type_mapping_t * services_pipe_type_mapping = NULL;
#endif

static const hal_aci_data_t setup_msgs[NB_SETUP_MESSAGES] PROGMEM =
  SETUP_MESSAGES_CONTENT;
static struct aci_state_t aci_state;
static hal_aci_evt_t aci_data;

/* Define how assert should function in the BLE library */
void __ble_assert(const char *file, uint16_t line)
{
  Serial.print("ERROR ");
  Serial.print(file);
  Serial.print(": ");
  Serial.print(line);
  Serial.print("\n");
  while (1);
}

void setup(void)
{
  Wire.begin();
  Serial.begin(9600);
  #ifdef DEBUG
  Serial.println(F("Arduino setup"));
  #endif

  /**
     Point ACI data structures to the the setup data that
     the nRFgo studio generated for the nRF8001
  */
  if (NULL != services_pipe_type_mapping)
  {
    aci_state.aci_setup_info.services_pipe_type_mapping =
      &services_pipe_type_mapping[0];
  }
  else
  {
    aci_state.aci_setup_info.services_pipe_type_mapping = NULL;
  }
  aci_state.aci_setup_info.number_of_pipes    = NUMBER_OF_PIPES;
  aci_state.aci_setup_info.setup_msgs         = (hal_aci_data_t*) setup_msgs;
  aci_state.aci_setup_info.num_setup_msgs     = NB_SETUP_MESSAGES;

  /*
    Tell the ACI library, the MCU to nRF8001 pin connections.
    The Active pin is optional and can be marked UNUSED
  */

  // connections are same as:
  // https://learn.adafruit.com/getting-started-with-the-nrf8001-bluefruit-le-breakout

  // See board.h for details
  aci_state.aci_pins.board_name = BOARD_DEFAULT;
  aci_state.aci_pins.reqn_pin   = 10;
  aci_state.aci_pins.rdyn_pin   = 2;
  aci_state.aci_pins.mosi_pin   = MOSI;
  aci_state.aci_pins.miso_pin   = MISO;
  aci_state.aci_pins.sck_pin    = SCK;

  // SPI_CLOCK_DIV8  = 2MHz SPI speed
  aci_state.aci_pins.spi_clock_divider      = SPI_CLOCK_DIV8;

  aci_state.aci_pins.reset_pin              = 9;
  aci_state.aci_pins.active_pin             = UNUSED;
  aci_state.aci_pins.optional_chip_sel_pin  = UNUSED;

  // Interrupts still not available in Chipkit
  aci_state.aci_pins.interface_is_interrupt = false;
  aci_state.aci_pins.interrupt_number       = 1;

  /** We reset the nRF8001 here by toggling the RESET line
      connected to the nRF8001
         and initialize the data structures required to setup the nRF8001
  */

  // The second parameter is for turning debug printing on
  // for the ACI Commands and Events so they be printed on the Serial
  lib_aci_init(&aci_state, false);
  /////////////////////////////////////////////////////////////////////////////////////////////////////
  TSL2572nit(GAIN_1X);
  /////////////////////////////////////////////////////////////////////////////////////////////////////
}

void aci_loop()
{
  static bool setup_required = false;

  // We enter the if statement only when there is a ACI event
  // available to be processed
  if (lib_aci_event_get(&aci_state, &aci_data))
  {
    aci_evt_t * aci_evt;
    aci_evt = &aci_data.evt;

    switch (aci_evt->evt_opcode) {
      case ACI_EVT_DEVICE_STARTED:
        {
          aci_state.data_credit_available =
            aci_evt->params.device_started.credit_available;

          switch (aci_evt->params.device_started.device_mode)
          {
            case ACI_DEVICE_SETUP:
              {
                #ifdef DEBUG
                Serial.println(F("Evt Device Started: Setup"));
                #endif
                
                aci_state.device_state = ACI_DEVICE_SETUP;
                setup_required = true;
              }
              break;

            case ACI_DEVICE_STANDBY:
              {
                aci_state.device_state = ACI_DEVICE_STANDBY;

                if (!broadcastSet) {
                  //lib_aci_open_adv_pipe(1);
                  //lib_aci_broadcast(0, 0x0100);
                  #ifdef DEBUG
                  Serial.println(F("Broadcasting started"));
                  #endif
                  broadcastSet = true;
                }

                // sleep_to_wakeup_timeout = 30;
                Serial.println(F("Evt Device Started: Standby"));
                if (aci_evt->params.device_started.hw_error) {
                  //Magic number used to make sure the HW error
                  //event is handled correctly.
                  delay(20);
                }
                else
                {
                  lib_aci_connect(30/* in seconds */,
                                  0x0100 /* advertising interval 100ms*/);
                  #ifdef DEBUG
                  Serial.println(F("Advertising started"));
                  #endif
                }
              }
              break;
          }
        }
        break; // case ACI_EVT_DEVICE_STARTED:

      case ACI_EVT_CMD_RSP:
        {
          //If an ACI command response event comes with an error -> stop
          if (ACI_STATUS_SUCCESS != aci_evt->params.cmd_rsp.cmd_status ) {
            // ACI ReadDynamicData and ACI WriteDynamicData
            // will have status codes of
            // TRANSACTION_CONTINUE and TRANSACTION_COMPLETE
            // all other ACI commands will have status code of
            // ACI_STATUS_SCUCCESS for a successful command
            #ifdef DEBUG
            Serial.print(F("ACI Status of ACI Evt Cmd Rsp 0x"));
            Serial.println(aci_evt->params.cmd_rsp.cmd_status, HEX);
            Serial.print(F("ACI Command 0x"));
            Serial.println(aci_evt->params.cmd_rsp.cmd_opcode, HEX);
            Serial.println(F("Evt Cmd respone: Error. "
                             "Arduino is in an while(1); loop"));
            #endif
            while (1);
          }
          else
          {
            // print command
            #ifdef DEBUG
            Serial.print(F("ACI Command 0x"));
            Serial.println(aci_evt->params.cmd_rsp.cmd_opcode, HEX);
            #endif
          }
        }
        break;

      case ACI_EVT_CONNECTED:
        {
          // The nRF8001 is now connected to the peer device.
          #ifdef DEBUG
          Serial.println(F("Evt Connected"));
          #endif
        }
        break;

      case ACI_EVT_DATA_CREDIT:
        {
          #ifdef DEBUG
          Serial.println(F("Evt Credit: Peer Radio acked our send"));
          #endif

          /** Bluetooth Radio ack received from the peer radio for
              the data packet sent.  This also signals that the
              buffer used by the nRF8001 for the data packet is
              available again.  We need to wait for the Confirmation
              from the peer GATT client for the data packet sent.
              The confirmation is the ack from the peer GATT client
              is sent as a ACI_EVT_DATA_ACK.  */
        }
        break;

      case ACI_EVT_DISCONNECTED:
        {
          // Advertise again if the advertising timed out.
          if (ACI_STATUS_ERROR_ADVT_TIMEOUT ==
              aci_evt->params.disconnected.aci_status) {
            #ifdef DEBUG
            Serial.println(F("Evt Disconnected -> Advertising timed out"));
            Serial.println(F("nRF8001 going to sleep"));
            #endif
            lib_aci_sleep();
            aci_state.device_state = ACI_DEVICE_SLEEP;
          }

          else
          {
            #ifdef DEBUG
            Serial.println(F("Evt Disconnected -> Link lost."));
            #endif
            
            lib_aci_connect(30/* in seconds */,
                            0x0050 /* advertising interval 50ms*/);

            #ifdef DEBUG
            Serial.println(F("Advertising started"));
            #endif
          }
        }
        break;

      case ACI_EVT_PIPE_STATUS:
        {
          Serial.println(F("Evt Pipe Status"));
          // check if the peer has subscribed to the
          // Temperature Characteristic

          notifyTemp = false;

        }
        break;

      case ACI_EVT_PIPE_ERROR:
        {
          // See the appendix in the nRF8001
          // Product Specication for details on the error codes
          #ifdef DEBUG
          Serial.print(F("ACI Evt Pipe Error: Pipe #:"));
          Serial.print(aci_evt->params.pipe_error.pipe_number, DEC);
          Serial.print(F("  Pipe Error Code: 0x"));
          Serial.println(aci_evt->params.pipe_error.error_code, HEX);
          #endif

          // Increment the credit available as the data packet was not sent.
          // The pipe error also represents the Attribute protocol
          // Error Response sent from the peer and that should not be counted
          //for the credit.
          if (ACI_STATUS_ERROR_PEER_ATT_ERROR !=
              aci_evt->params.pipe_error.error_code) {
            aci_state.data_credit_available++;
          }
        }
        break;

      case ACI_EVT_DATA_ACK:
        {
          #ifdef DEBUG
          Serial.println(F("Attribute protocol ACK for "
                           "Temp. measurement Indication"));
          #endif
        }
        break;

      case ACI_EVT_HW_ERROR:
        {
          Serial.println(F("HW error: "));
          Serial.println(aci_evt->params.hw_error.line_num, DEC);

          for (uint8_t counter = 0; counter <= (aci_evt->len - 3); counter++)
          {
            Serial.write(aci_evt->params.hw_error.file_name[counter]);
          }
          Serial.println();
          lib_aci_connect(30/* in seconds */,
                          0x0100 /* advertising interval 100ms*/);

          #ifdef DEBUG
          Serial.println(F("Advertising started"));
          #endif
        }
        break;

      default:
        {
          #ifdef DEBUG
          Serial.print(F("Evt Opcode 0x"));
          Serial.print(aci_evt->evt_opcode, HEX);
          Serial.println(F(" unhandled"));
          #endif
        }
        break;
    }
  }
  else
  {
    //  No event in the ACI Event queue
  }

  /* setup_required is set to true when the device starts up and
     enters setup mode.
        It indicates that do_aci_setup() should be
     called. The flag should be cleared if
        do_aci_setup() returns ACI_STATUS_TRANSACTION_COMPLETE.  */

  if (setup_required)
  {
    if (SETUP_SUCCESS == do_aci_setup(&aci_state))
    {
      setup_required = false;
    }
  }
}

#define SEQ_NUM 2
#define LMETERS 4

#define LUX_PERIOD 1000	// period between lux measurements (ms)
#define SERIAL_PERIOD 200

// for timing of events
unsigned long time = 0;
unsigned long last_time = 0;

boolean act = false;	// true if time to read ambient light
uint8_t timestamp = 0;
float lux = 0;

bool recv = true;	// true if wireless controller has acknowledged the
// the last packet sent

/**
   Reads from the light sensor every one second and writes it to the Wifi
   transmitter via UART.  If the Wifi transmitter is unable to transmit the
   data or does not respond in time, will attempt to send via Bluetooth.
*/
void loop()
{
  aci_loop();

  time = millis();
  if (time - last_time > LUX_PERIOD) {
    last_time = time;
    act = true;
  }

  // allow 200 ms for lux data to be sent
  if (time - last_time > SERIAL_PERIOD) {

    while (Serial.available()) {

      // check each character if success/failure is not determined
      if (!recv) {
        int tst = Serial.read();

        // successful Wifi transmission
        if (tst == '+') {
          recv = true;
        }

        // unsuccessful Wifi transmission
        else if (tst == '-') {
          #ifdef DEBUG
          Serial.println("Wifi transmission failed.");
          #endif

          // check bluetooth readiness
          if (broadcastSet) {
            send_lux_via_ble();
            //Serial.println("\ntest");
          }

          recv = true;
        }

        // discard extra characters for safety
        // NOTE: This is so that next time a light data packet is sent we are
        // guaranteed to not be reading chars that were received before that
        // packet was sent.
      } else {

        //TODO: Check that using null as the buffer address discards chars as planned.
        Serial.readBytes((byte*)0, Serial.available());
      }
    }
  }

  // read ambient light
  if (act) { //I eventually figured out how to actually check it I'm connected.

    // last chance to send previous lux, so check if it was acknowledged
    if (!recv) {
      #ifdef DEBUG
      Serial.println("Wifi module not responding...");
      #endif

      if (broadcastSet) {
        
        send_lux_via_ble();
      }
    }

    lux = Tsl2572ReadAmbientLight();
    act = false;

    // print lux packet to serial port
    Serial.print("lux ");
    Serial.print(DEVICE);
    Serial.print(" ");
    Serial.print(lux);
    Serial.print(" ");
    Serial.print(timestamp);
    Serial.print(" $");
    timestamp++;
    recv = false;
  }
}

/**
   Send a lux packet via bluetooth.
*/
void send_lux_via_ble() {

  #ifdef DEBUG
  Serial.println("Sending lux data via bluetooth.");
  #endif
  
  uint32_t l2 = __builtin_bswap32((uint32_t)lux);
  write_int_to_pipe_3(l2, LMETERS);
  write_int_to_pipe(timestamp, SEQ_NUM);
}

void write_int_to_pipe(uint8_t integer, int pipe) {
  lib_aci_set_local_data(&aci_state,
                         pipe,
                         (uint8_t*)&integer, 1);

}

void write_int_to_pipe_2(uint16_t integer, int pipe) {
  lib_aci_set_local_data(&aci_state,
                         pipe,
                         (uint8_t*)((&integer)), 2);

}

void write_int_to_pipe_3(uint32_t integer, int pipe) {
  lib_aci_set_local_data(&aci_state,
                         pipe,
                         (uint8_t*)((&integer)), 4);

}

void write_to_pipe(float f, int pipe) {
  //I still have no idea why this even compiles
  //lib_aci_set_local_data(&aci_state,
  //                      pipe,
  //                      (uint8_t*)&f, 4);
  //magic number 4 is number of bytes our datatype uses
  //almost missed it
}




void TSL2572nit(uint8_t gain)
{
  Tsl2572RegisterWrite( 0x0F, gain );//set gain
  Tsl2572RegisterWrite( 0x01, 0xED );//51.87 ms
  Tsl2572RegisterWrite( 0x00, 0x03 );//turn on
  if (GAIN_DIVIDE_6)
    Tsl2572RegisterWrite( 0x0D, 0x04 );//scale gain by 0.16
  if (gain == GAIN_1X)gain_val = 1;
  else if (gain == GAIN_8X)gain_val = 8;
  else if (gain == GAIN_16X)gain_val = 16;
  else if (gain == GAIN_120X)gain_val = 120;
}

void Tsl2572RegisterWrite( byte regAddr, byte regData )
{
  Wire.beginTransmission(TSL2572_I2CADDR);
  Wire.write(0x80 | regAddr);
  Wire.write(regData);
  Wire.endTransmission();
}

float Tsl2572ReadAmbientLight()
{
  uint8_t data[4];
  int c0, c1;
  float lux1, lux2, cpl;

  Wire.beginTransmission(TSL2572_I2CADDR);
  Wire.write(0xA0 | 0x14);
  Wire.endTransmission();
  Wire.requestFrom(TSL2572_I2CADDR, 4);
  for (uint8_t i = 0; i < 4; i++)
    data[i] = Wire.read();

  c0 = data[1] << 8 | data[0];
  c1 = data[3] << 8 | data[2];

  //see TSL2572 datasheet
  cpl = 51.87 * (float)gain_val / 60.0;
  if (GAIN_DIVIDE_6) cpl /= 6.0;
  lux1 = ((float)c0 - (1.87 * (float)c1)) / cpl;
  lux2 = ((0.63 * (float)c0) - (float)c1) / cpl;
  cpl = max(lux1, lux2);
  return max(cpl, 0.0);
}
