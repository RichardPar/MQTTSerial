#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>


#include "mqtt.h"
#include "utils.h"
#include "globals.h"
#include "mqttserial.h"


#define PORT_CLOSED -1
#define PORT_OPEN 0;


int g_portOpen = PORT_CLOSED;
char g_serialPort[255];
int  g_portfd;
pthread_t serial_thread;
int   tid;
volatile int RUNNING;


int mqtt_serial_online_callback(char *node,char *msg, int len, void *p);
int mqtt_serial_open_callback(char *node,char *msg, int len, void *p);
int mqtt_serial_read_callback(char *node,char *msg, int len, void *p);
int mqtt_serial_close_callback(char *node,char *msg, int len, void *p);
int mqtt_serial_write_callback(char *node,char *msg, int len, void *p);
int serial_setparameters (int fd, int speed, int parity);
void *serial_read_thread(void *p);
void set_blocking_mode (int fd, int should_block);
void start_read_thread(void);


void start_serial(void *pvt)
{
  char node[255];
  g_portfd=-1;
  RUNNING=-1;
  sprintf(g_serialPort,SERIALDEVICE);


  sprintf(node,"%s/CHECK",UID);
  mqtt_register_callback(node,&mqtt_serial_online_callback,pvt);
  sprintf(node,"ALL/CHECK");
  mqtt_register_callback(node,&mqtt_serial_online_callback,pvt);


  sprintf(node,"%s/OPEN",UID);
  mqtt_register_callback(node,&mqtt_serial_open_callback,pvt);
  sprintf(node,"ALL/OPEN");
  mqtt_register_callback(node,&mqtt_serial_open_callback,pvt);



  sprintf(node,"%s/READ",UID);
  mqtt_register_callback(node,&mqtt_serial_read_callback,pvt);
  sprintf(node,"ALL/READ");
  mqtt_register_callback(node,&mqtt_serial_read_callback,pvt);


  sprintf(node,"%s/WRITE",UID);
  mqtt_register_callback(node,&mqtt_serial_write_callback,pvt);
  sprintf(node,"ALL/WRITE");
  mqtt_register_callback(node,&mqtt_serial_write_callback,pvt);


  sprintf(node,"%s/CLOSE",UID);
  mqtt_register_callback(node,&mqtt_serial_close_callback,pvt);
  sprintf(node,"ALL/CLOSE");
  mqtt_register_callback(node,&mqtt_serial_close_callback,pvt);


}


int mqtt_serial_online_callback(char *node,char *msg, int len, void *p)
{
    char outnode[128];
    char outString[128];
    int iInput;

    iInput = getInteger(msg,len);

    sprintf(outString,"%d,ONLINE",iInput);
    sprintf(outnode,"CHECK");
    mqtt_writeresponse(outnode,outString,0);

    return 0;
}


int mqtt_serial_close_callback(char *node,char *msg, int len, void *p)
{
    char outnode[128];
    char outString[128];

    RUNNING=-1;
    return 0;
}

int mqtt_serial_open_callback(char *node,char *msg, int len, void *p)
{
    char outnode[128];
    char outString[128];
    int fd=-1;
    int rc=-1;

    if (g_portfd < 0)
     {
      fd = open (g_serialPort, O_RDWR | O_NOCTTY | O_SYNC);
      if (fd >= 0)
      {
        //The values for speed are B115200, B230400, B9600, B19200, B38400, B57600, B1200, B2400, B4800 .. more
        rc = serial_setparameters(fd, B230400, 0); 
      }

      if (rc < 0)
      {
       close(fd);
      } else
      {
       g_portfd = fd;
      }
    } 

    if (g_portfd >= 0)
        set_blocking_mode (g_portfd,0);
    

    sprintf(outString,"%d",g_portfd);
    sprintf(outnode,"OPEN");
    mqtt_writeresponse(outnode,outString,0);
    start_read_thread();


    return 0;
}

int mqtt_serial_write_callback(char *node,char *msg, int len, void *p)
{
    char outnode[128];
    char outString[128+4096];
    char payload[4096];
    int  iInput,n;



    if (g_portfd >= 0)
    {
      n = read (g_portfd,msg,len);  // read up to iInput characters if ready to read
    } else
    {
     n=-1;
     payload[0]=0;
    }


    sprintf(outString,"%d,WRITTEN",n);
    sprintf(outnode,"WRITE");
    mqtt_writeresponse(outnode,outString,0);

    return 0;
}

int mqtt_serial_read_callback(char *node,char *msg, int len, void *p)
{
    char outnode[128];
    char outString[128+4096];
    char payload[4096];
    int  iInput,n;



    if (g_portfd >= 0)
    {
      iInput = getInteger(msg,len);

      if (iInput > 4095)
        iInput=4095;

      n = read (g_portfd, payload,iInput);  // read up to iInput characters if ready to read
    } else
    {
     n=-1;
     payload[0]=0;
    }


    sprintf(outString,"%d,%s",n,payload);
    sprintf(outnode,"READ");
    mqtt_writeresponse(outnode,outString,0);

    return 0;
}

int serial_setparameters (int fd, int speed, int parity)
{
        struct termios tty;
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                printf ("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}


void set_blocking_mode (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf("error %d setting term attributes", errno);
}

void start_read_thread(void)
{ 
   tid = pthread_create(&serial_thread, NULL, serial_read_thread,NULL);
}


void *serial_read_thread(void *p)
{

  char outnode[128];
  char outString[128+8192];
  char buffer[8192];
  int  bufferptr;
  char inchar;

  int iInput;

  RUNNING=1;
  bufferptr=0;
  buffer[0] = 0;

  while (RUNNING==1)
  {

   int len;

   len = read (g_portfd, &inchar,1);
   if (len >= 1)
   {
     if ((inchar != 10) && (inchar != 13)) 
     {
        buffer[bufferptr] = inchar;
        bufferptr++;
        
        if (bufferptr > 8190)
          {
           buffer[0]=0;
           bufferptr=0;
          }
     } else
     {


      if (bufferptr > 0)
      {
        sprintf(outString,"%d,%s",bufferptr,buffer);
        sprintf(outnode,"RECV");
        mqtt_writeresponse(outnode,outString,0);
        buffer[0] = 0;
        bufferptr=0;
      }
     }
   } else
   {
     usleep(100);
   }
  }


  sprintf(outString,"CLOSED");
  sprintf(outnode,"CLOSED");
  mqtt_writeresponse(outnode,outString,0);
  close(g_portfd);
  g_portfd=-1; 
  printf("Port reader terminated\n");
  pthread_exit(0);
}
