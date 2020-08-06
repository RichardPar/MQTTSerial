
#include <stdio.h>
#include <string.h>
#include <statgrab.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>



#include "globals.h"
#include "utils.h"
#include "mqtt.h"
#include "mqttserial.h"

int main(int argc, char *argv[])
{
    BS = generateBootSession();
    generate_uid(UID);

    printf("UID is %s\r\n",UID);
    strcpy(VENDOR,"MINE");
    strcpy(PRODUCT,"LINUXBOXEN");

    mqtt_initfuncs();
    mqtt_connect(ADDRESS,CLIENTID);

    start_serial(NULL);

    while (1)
   {
      sleep(5);
   }



 return 0;
}


