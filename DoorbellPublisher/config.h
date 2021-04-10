#include "config_wifi.h" /* include WiFi settings from separate file */

#define MQTT_SERVER_IP         "192.168.0.4" /* shall be a static IP address, not a hostname */
#define MQTT_PORT              1883
#define MQTT_CLIENT_ID         "DoorbellPublisher"
#define MQTT_TOPIC_CONNECTION  "doorbell_connection"
#define MQTT_MSG_HEARTBEAT     "heartbeat"
#define MQTT_MSG_CONNECT       "connected"
#define MQTT_TOPIC_PROD        "doorbell"
#define MQTT_MSG_DOORBELL      "dingdong"
#define ANALOG_INPUT_PIN        A0

/* Sensor loop delay to busy wait and currently get a sample interval of approx. 3 ms, i.e. a sampling frequency of 333 Hz. */
#define SENSOR_LOOP_DELAY       (3u) /* loop delay in milliseconds */

#define SENSOR_THRESHOLD        (40.0f)

#define SENSOR_CNT_LIMIT        (15u)

#define SENSOR_CNT_DOWN         (800u)

#define TIMEOUT_RESET_VAL       (5u) /* heartbeat interval in seconds */

#define SERIAL_BAUDRATE         (115200u)

#define DEBUG                   (1u)
