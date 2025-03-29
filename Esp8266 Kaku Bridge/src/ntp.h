#ifndef MYNTP_h
#define MYNTP_h

#include <Arduino.h>
#include <TimeLib.h>
#include <WiFiUdp.h>


void sendNTPpacket(IPAddress &address);
time_t getNtpTime();
void setupNTP();

#endif