#ifndef WATCHDOGAVR_H
#define WATCHDOGAVR_H

#include <avr/wdt.h>

class WatchdogAVR {
public:
    int enable(int maxCountdownMS = 0) {
        // Enable watchdog timer for at most the specified countdown in
        // milliseconds.  For the default countdown value of 0 use the biggest
        // countdown possible (8 seconds for an AVR).
        int wdto;
        int actualMS;
        // Note the order of these if statements from highest to lowerest  is 
        // important so that control flow cascades down to the right value based
        // on its position in the range of discrete timeouts.
        if ((maxCountdownMS >= 8000) || (maxCountdownMS == 0)) {
            wdto     = WDTO_8S;
            actualMS = 8000;
        }
        else if (maxCountdownMS >= 4000) {
            wdto     = WDTO_4S;
            actualMS = 4000;
        }
        else if (maxCountdownMS >= 2000) {
            wdto     = WDTO_2S;
            actualMS = 2000;
        }
        else if (maxCountdownMS >= 1000) {
            wdto     = WDTO_1S;
            actualMS = 1000;
        }
        else if (maxCountdownMS >= 500) {
            wdto     = WDTO_500MS;
            actualMS = 500;
        }
        else if (maxCountdownMS >= 250) {
            wdto     = WDTO_250MS;
            actualMS = 250;
        }
        else if (maxCountdownMS >= 120) {
            wdto     = WDTO_120MS;
            actualMS = 120;
        }
        else if (maxCountdownMS >= 60) {
            wdto     = WDTO_60MS;
            actualMS = 60;
        }
        else if (maxCountdownMS >= 30) {
            wdto     = WDTO_30MS;
            actualMS = 30;
        }
        else {
            wdto     = WDTO_15MS;
            actualMS = 15;
        }
        // Enable the watchdog and return the actual countdown value.
        wdt_enable(wdto);
        return actualMS;
    }

    void reset() {
        // Reset the watchdog.
        wdt_reset();
    }

    void disable() {
        // Disable the watchdog.
        wdt_disable();
    }
};

#endif