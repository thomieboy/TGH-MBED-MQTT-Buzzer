/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/
 
 /**
  This is a sample program to illustrate the use of the MQTT Client library
  on the mbed platform.  The Client class requires two classes which mediate
  access to system interfaces for networking and timing.  As long as these two
  classes provide the required public programming interfaces, it does not matter
  what facilities they use underneath. In this program, they use the mbed
  system libraries.
 
 */

#include "mbed.h"
#include "EthernetInterface.h"

#include "C12832_lcd.h"
C12832_LCD lcd;

#include "FP.cpp"
#include "MQTTClient.h"



class IPStack 
{
public:    
    IPStack()
    {
        eth.init();                          // Use DHCP
        eth.connect();
        mysock.set_blocking(false, 1000);    // 1 second Timeout 
    }
    
    int connect(char* hostname, int port)
    {
        return mysock.connect(hostname, port);
    }

    int read(char* buffer, int len, int timeout)
    {
        mysock.set_blocking(false, timeout);  
        return mysock.receive(buffer, len);
    }
    
    int write(char* buffer, int len, int timeout)
    {
        mysock.set_blocking(false, timeout);  
        return mysock.send(buffer, len);
    }
    
    int disconnect()
    {
        return mysock.close();
    }
    
private:

    EthernetInterface eth;
    TCPSocketConnection mysock; 
    
};


class Countdown
{
public:
    Countdown()
    {
        t = Timer();   
    }
    
    Countdown(int ms)
    {
        t = Timer();
        countdown_ms(ms);   
    }
    
    
    bool expired()
    {
        return t.read_ms() >= interval_end_ms;
    }
    
    void countdown_ms(int ms)  
    {
        t.stop();
        interval_end_ms = ms;
        t.reset();
        t.start();
    }
    
    void countdown(int seconds)
    {
        countdown_ms(seconds * 1000);
    }
    
    int left_ms()
    {
        return interval_end_ms - t.read_ms();
    }
    
private:
    Timer t;
    int interval_end_ms; 
};

int arrivedcount = 0;

void messageArrived(MQTT::Message* message)
{
    lcd.printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message->qos, message->retained, message->dup, message->id);
    lcd.printf("Payload %.*s\n", message->payloadlen, (char*)message->payload);
    ++arrivedcount;
}


int main(int argc, char* argv[])
{   
    IPStack ipstack = IPStack();
    float version = 0.42;
    char* topic = "mbed-sample";
    
    lcd.printf("Version is %f\n", version);
              
    MQTT::Client<IPStack, Countdown> client = MQTT::Client<IPStack, Countdown>(ipstack);
    
    char* hostname = "m2m.eclipse.org";
    int port = 1883;
    lcd.printf("Connecting to %s:%d\n", hostname, port);
    int rc = ipstack.connect(hostname, port);
    if (rc != 0)
        lcd.printf("rc from TCP connect is %d\n", rc);
 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-icraggs";
    rc = client.connect(&data);
    if (rc != 0)
        lcd.printf("rc from MQTT connect is %d\n", rc);
    
    rc = client.subscribe(topic, MQTT::QOS1, messageArrived);   
    if (rc != 0)
        lcd.printf("rc from MQTT subscribe is %d\n", rc);

    MQTT::Message message;

    // QoS 0
    char buf[100];
    sprintf(buf, "Hello World!  QoS 0 message from app version %f\n", version);
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, &message);
    while (arrivedcount == 0)
        client.yield(100);
        
    // QoS 1
    sprintf(buf, "Hello World!  QoS 1 message from app version %f\n", version);
    message.qos = MQTT::QOS1;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, &message);
    while (arrivedcount == 1)
        client.yield(100);
        
    // QoS 2
    sprintf(buf, "Hello World!  QoS 2 message from app version %f\n", version);
    message.qos = MQTT::QOS2;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, &message);
    while (arrivedcount == 2)
        client.yield(100);
    
    rc = client.unsubscribe(topic);
    if (rc != 0)
        lcd.printf("rc from unsubscribe was %d\n", rc);
    
    rc = client.disconnect();
    if (rc != 0)
        lcd.printf("rc from disconnect was %d\n", rc);
    
    ipstack.disconnect();
    
    lcd.printf("Finishing with %d messages received\n", arrivedcount);
    
    return 0;
}
