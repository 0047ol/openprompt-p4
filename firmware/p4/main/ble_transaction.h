#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* A central may multiplex several browser origins onto one ATT connection.
 * Tokens reserve fragmented uploads and response cursors, not authentication.
 * An abandoned transaction expires without disconnecting other viewers. */
typedef struct {
    uint8_t token[6];
    char client[33];
    int64_t until;
} tp_ble_transaction_t;
#define TP_BLE_TRANSACTION_US INT64_C(8000000)
static inline void tp_ble_client_decode(char client[25],const uint8_t data[12])
{
    const char *hex="0123456789abcdef";
    for(size_t i=0;i<12;i++){client[2*i]=hex[data[i]>>4];client[2*i+1]=hex[data[i]&15];}client[24]=0;
}
static inline bool tp_ble_transaction_active(const tp_ble_transaction_t *t,int64_t now)
{return t->until>now;}
static inline bool tp_ble_transaction_claim(tp_ble_transaction_t *t,const uint8_t *data,size_t n,int64_t now)
{
    if(n!=19)return false;
    uint8_t any=0;for(size_t i=1;i<7;i++)any|=data[i];if(!any)return false;
    char client[25];tp_ble_client_decode(client,data+7);
    if(tp_ble_transaction_active(t,now)&&(memcmp(t->token,data+1,6)||strcmp(t->client,client)))return false;
    memcpy(t->token,data+1,6);memcpy(t->client,client,sizeof(client));t->until=now+TP_BLE_TRANSACTION_US;return true;
}
static inline bool tp_ble_transaction_release(tp_ble_transaction_t *t,const uint8_t *data,size_t n)
{
    if(n!=7||memcmp(t->token,data+1,6))return false;
    memset(t,0,sizeof(*t));return true;
}
static inline bool tp_ble_transaction_unwrap(tp_ble_transaction_t *t,uint8_t *data,uint16_t *n,int64_t now)
{
    if(*n<=7||!tp_ble_transaction_active(t,now)||memcmp(t->token,data+1,6))return false;
    *n-=7;memmove(data,data+7,*n);t->until=now+TP_BLE_TRANSACTION_US;return true;
}
