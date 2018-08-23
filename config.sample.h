#define WIFI_SSID        "...."
#define WIFI_PASS        "...."
#define DEVICE_NAME      "Disco Lights"
#define LED_DEVICE_NAME  "ESP LED"
#define HOST_NAME        "espGrowLights"                // The DNS name
const int lightOnHour =   7;                            // Auto on hour
const int lightOffHour = 19;                            // Auto off hour
static const char ntpServerName[] = "us.pool.ntp.org";  // NTP server
const int timeZone =     -4;                            // Eastern Daylight Time (USA)
// const int timeZone =    -5;                            // Eastern Standard Time (USA)
// const int timeZone =    0;                             // UTC


// Arduino settings
#define SERIAL_BAUDRATE     115200
#define LED                 LED_BUILTIN
#define TOP_GROWLIGHT       D5
#define BOTTOM_GROWLIGHT    D6
#define BUTTON              D7
