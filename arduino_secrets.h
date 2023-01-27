// SSID Credentials
#define SECRET_SSID "YourSSIDNameHere"      // Your SSID
#define SECRET_PASS "YourSSIDPasswordHere"  // Your SSID Password

// NTP settings
#define NTPSERVER "pool.ntp.org"  // Your NTP server address here e.g. pool.ntp.org or your router/firewall IP with NTP abilities.

/* Timezone settings
// Use https://github.com/JChristensen/Timezone
// No need to set up an "Hours Offset Index" in the nixie clock with the buttons
// Set to 0 if you will use the TimeZone settings for DST
*/
//      myRule {"abbrev", week, dow, month, hour, offset}
#define TZ_DST {"CEST", Last, Sun, Mar, 2, 120}    // TZ with DST e.g. Central European Summer Time
#define TZ_DEF {"CET ", Last, Sun, Oct, 3, 60}     // TZ default (no DST) e.g. Central European Standard Time