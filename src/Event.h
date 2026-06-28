#ifndef __EVENT__H
#define __EVENT__H

#include <Arduino.h>

struct Event
{
    uint8_t hour, minute;
    bool open, once;
};


#endif