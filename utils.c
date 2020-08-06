#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <linux/unistd.h> /* for _syscallX macros/related stuff */
#include <linux/kernel.h> /* for struct sysinfo */
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

void sleep_with_break(int* interval)
{
    int i = *interval;
    int loops = i * 2;

    while(loops-- > 0) {
        usleep(500 * 1000);

        if(i != *interval) {
            printf("Timer changed... break");
            break;
        }
    }

    return;
}

unsigned long int getEpochms()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
}

long get_uptime()
{
    struct sysinfo s_info;
    int error = sysinfo(&s_info);
    if(error != 0) {
        printf("code error = %d\n", error);
    }
    return s_info.uptime;
}

// This will only use 20 characters!
int getInteger(char* msg, int len)
{
    int iInput;
    char inMsg[20];
    int inLen;

    if(len >= 20) {
        inLen = 19;
    } else
        inLen = len;

    memset(inMsg, 0, 20);
    memcpy(inMsg, msg, inLen);

    iInput = atoi(inMsg);

    return iInput;
}

void generate_uid(char uid[20])
{
    char buf[8192] = { 0 };
    struct ifconf ifc = { 0 };
    struct ifreq* ifr = NULL;
    int sck = 0;
    int nInterfaces = 0;
    int i = 0;
    char macp[19];
    char inmac[6];
    struct ifreq* item;
//    struct sockaddr* addr;
    char nullmac[6] = {0,0,0,0,0,0};


    /* Get a socket handle. */
    sck = socket(PF_INET, SOCK_DGRAM, 0);
    if(sck < 0) {
        perror("socket");
        return;
    }

    /* Query available interfaces. */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(sck, SIOCGIFCONF, &ifc) < 0) {
        perror("ioctl(SIOCGIFCONF)");
        return;
    }

    /* Iterate through the list of interfaces. */
    ifr = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

    memset(inmac, 0, 6);

    printf("Detected %d ethernet interfaces\r\n",nInterfaces);

    for(i = 0; i < nInterfaces; i++) {
        item = &ifr[i];
        
    
 //       addr = &(item->ifr_addr);

        /* Get the MAC address */
        if(ioctl(sck, SIOCGIFHWADDR, item) < 0) {
            perror("ioctl(SIOCGIFHWADDR)");
            continue;
        }

        // You dont want to use ZERO mac addresses... they are useless
        if (!memcmp(item->ifr_hwaddr.sa_data,nullmac,6))
           continue;


        /* display result */

        inmac[0] = inmac[0] ^ item->ifr_hwaddr.sa_data[0];
        inmac[1] = inmac[1] ^ item->ifr_hwaddr.sa_data[1];
        inmac[2] = inmac[2] ^ item->ifr_hwaddr.sa_data[2];
        inmac[3] = inmac[3] ^ item->ifr_hwaddr.sa_data[3];
        inmac[4] = inmac[4] ^ item->ifr_hwaddr.sa_data[4];
        inmac[5] = inmac[5] ^ item->ifr_hwaddr.sa_data[5];
        
        
        sprintf(macp,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned char)item->ifr_hwaddr.sa_data[0],
            (unsigned char)item->ifr_hwaddr.sa_data[1],
            (unsigned char)item->ifr_hwaddr.sa_data[2],
            (unsigned char)item->ifr_hwaddr.sa_data[3],
            (unsigned char)item->ifr_hwaddr.sa_data[4],
            (unsigned char)item->ifr_hwaddr.sa_data[5]);
        printf("MAC%s\r\n",macp);
        

    }

    sprintf(macp,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned char)inmac[0],
            (unsigned char)inmac[1],
            (unsigned char)inmac[2],
            (unsigned char)inmac[3],
            (unsigned char)inmac[4],
            (unsigned char)inmac[5]);

    strcpy(uid, macp);

    return;
}


int getCPUSerial()
{
   FILE *f = fopen("/proc/cpuinfo", "r");
   if (!f) {
      return 1;
   }
   
   char line[256];
   int serial;
   while (fgets(line, 256, f)) {
      if (strncmp(line, "Serial", 6) == 0) {
         char serial_string[16 + 1];
         serial = atoi(strcpy(serial_string, strchr(line, ':') + 2));
      }
   }

   return serial;
}

unsigned long generateBootSession()
{
   size_t r;
   FILE *f = fopen("/dev/urandom", "r");
   if (!f) {
      return 1;
   }
   
   unsigned long serial;
   r = fread(&serial,sizeof(unsigned long),1,f);
   fclose(f);

   return serial;
}