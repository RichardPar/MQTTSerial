
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#include "globals.h"
#include "utils.h"
#include "mqtt.h"
#include "list.h"

void connlost(void* context, char* cause);
int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);
void onSubscribe(void* context, MQTTAsync_successData* response);
void onSubscribeFailure(void* context, MQTTAsync_failureData* response);

void mqtt_initfuncs(void)
{
    mqtt_funcs = 0;
    
}


void mqtt_subscribe(char *vendor, char *product, char *tag,void *ptr)
{
    int rc;
   
   	MQTTAsync_responseOptions ropts = MQTTAsync_responseOptions_initializer;
    
    ropts.onSuccess = onSubscribe;
	ropts.onFailure = onSubscribeFailure;
	ropts.context = ptr;

    rc = MQTTAsync_subscribe(client, tag, QOS, &ropts);    
    printf("Subscribing to %s (rc %d)\r\n",tag,rc);
        
}


void mqtt_register_callback(char *inNode,void *func, void *ptr)
{
    mqtt_callback_ll *tmp;
    char node[255];
    
    
   //	MQTTAsync_responseOptions ropts = MQTTAsync_responseOptions_initializer;

    //    VENDOR/PRODUCT/
    sprintf(node,"REQ/%s/%s/%s",VENDOR,PRODUCT,inNode);
    
    tmp = malloc(sizeof(mqtt_callback_ll));
    strcpy(tmp->node,node);
    tmp->functionPtr = func;
    tmp->dataPtr= ptr;
    tmp->subscribed=0;
    DL_APPEND(mqtt_funcs, tmp);
    
    
    if (MQTTAsync_isConnected(client)) {
       mqtt_subscribe(VENDOR,PRODUCT,inNode,tmp);
    }
    
    return;
}

void onPublishFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Publish failed, rc %d\n", response ? -1 : response->code);
	 
}


void onPublish(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;

//	printf("Published\r\n");
}


int mqtt_writeresponse(char *intag, char *message, int transaction)
{
    char outMessage[8192];
    char outTag[255];
    
    sprintf(outMessage,"%lu,%u,%lu,%lu,%s",BS,transaction,getEpochms(),get_uptime(),message);
    sprintf(outTag,"RESP/%s/%s/%s/%s",VENDOR,PRODUCT,UID,intag);

    mqtt_writedata(outTag,outMessage);
    return 0;
}


int mqtt_writedata(char* tag, char* message)
{
   int rc;
   
    printf("TAG : %s [%s] \r\n",tag,message);
    
    MQTTAsync_responseOptions pub_opts = MQTTAsync_responseOptions_initializer;
    pub_opts.onSuccess = onPublish;
    pub_opts.onFailure = onPublishFailure;
    
	rc = MQTTAsync_send(client, tag, strlen(message), message,QOS,1, &pub_opts);
		
    return rc;
}


void onSubscribe(void* context, MQTTAsync_successData* response)
{
    
    printf("Message subscribed\r\n");
    mqtt_callback_ll *tmp = context;

    tmp->subscribed=1;

}


void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
   mqtt_callback_ll *tmp = context;

	printf("Subscribe failed, rc %d\n", response->code);
    tmp->subscribed=0;
}

void onConnect(void* context, MQTTAsync_successData* response)
{
    mqtt_callback_ll *tmp;

    printf("Connected to Host\r\n");

    DL_FOREACH(mqtt_funcs,tmp) {
        if (tmp->subscribed==0)
           mqtt_subscribe(VENDOR,PRODUCT,tmp->node,tmp);
    }
}


int mqtt_connect(char* url, char* clientid)
{
    int rc;
    //========= MQTT ============================//
    //MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

    
    MQTTAsync_create(&client, url, clientid, MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;
    conn_opts.retryInterval = 5;
    conn_opts.onSuccess = onConnect;
    
    
    rc = MQTTAsync_create(&client, ADDRESS,CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, client, connlost, messageArrived, NULL);
    
    
    if((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    return 0;
}

void connlost(void* context, char* cause)
{
    
    mqtt_callback_ll *tmp;

    DL_FOREACH(mqtt_funcs,tmp) {
        tmp->subscribed=0;
    }
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
    char* payloadptr;
    mqtt_callback_ll *elt;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;
    for(i = 0; i < message->payloadlen; i++) {
        putchar(*payloadptr++);
    }
    
    DL_FOREACH(mqtt_funcs,elt) {
        
      if (topicLen == strlen(elt->node))
      {  
        if (!memcmp(elt->node,topicName,strlen(elt->node)+1))
        {
            elt->functionPtr(elt->node,message->payload,message->payloadlen,elt->dataPtr);
        }    
      } // topicLength Match
    }
    
    putchar('\n');
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}


