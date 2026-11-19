#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Stored in NVS independently of the current DHCP lease. Host-order integers
 * make subnet validation independent of the target's byte order. */
typedef struct {
    uint32_t version;
    uint8_t manual;
    uint8_t mdns_enabled;
    uint8_t reserved[2];
    char ap[16], ip[16], mask[16], gateway[16], dns[16], hostname[64];
} tp_network_address_t;

static inline bool tp_hostname_valid(const char *name)
{
    if(!*name)return true; /* Empty disables mDNS. */
    unsigned n=0;
    for(;name[n];++n){
        char c=name[n];
        if(n>=63||!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
    }
    return n&&name[0]!='-'&&name[n-1]!='-';
}

static inline bool tp_ipv4(const char *text, uint32_t *value)
{
    if (!text || !*text) return false;
    uint32_t result = 0;
    for (unsigned part = 0; part < 4; ++part) {
        unsigned n = 0, digits = 0;
        const char *start = text;
        while (*text >= '0' && *text <= '9') {
            n = n * 10 + (*text++ - '0');
            if (++digits > 3 || n > 255) return false;
        }
        if (!digits || (digits > 1 && *start == '0')) return false;
        result = (result << 8) | n;
        if (part < 3) { if (*text++ != '.') return false; }
        else if (*text) return false;
    }
    *value = result;
    return true;
}

static inline bool tp_unicast(uint32_t ip)
{
    unsigned first = ip >> 24;
    return first && first != 127 && first < 224 &&
           (ip & UINT32_C(0xffff0000)) != UINT32_C(0xa9fe0000);
}

static inline bool tp_subnet_host(uint32_t ip, uint32_t mask)
{
    return tp_unicast(ip) && (ip & ~mask) && (ip & ~mask) != ~mask;
}

static inline bool tp_network_address_valid(const tp_network_address_t *c)
{
    uint32_t ap, ip, mask, gateway, dns;
    if (c->version != 1 || c->manual > 1 || c->mdns_enabled > 1 || (c->mdns_enabled&&!c->hostname[0]) || !tp_hostname_valid(c->hostname) || !tp_ipv4(c->ap, &ap) ||
        !tp_subnet_host(ap, UINT32_C(0xffffff00))) return false;
    if (!c->manual) return true;
    if (!tp_ipv4(c->ip, &ip) || !tp_ipv4(c->mask, &mask) ||
        !tp_ipv4(c->gateway, &gateway) || !tp_ipv4(c->dns, &dns)) return false;
    uint32_t hosts = ~mask;
    if (!mask || hosts < 3 || (hosts & (hosts + 1))) return false;
    if (!tp_subnet_host(ip, mask) || !tp_subnet_host(gateway, mask) ||
        !tp_unicast(dns) || ip == gateway || (ip & mask) != (gateway & mask)) return false;
    /* AP is /24. Reject overlapping routes, including a larger station subnet. */
    uint32_t common = mask & UINT32_C(0xffffff00);
    return (ap & common) != (ip & common);
}
