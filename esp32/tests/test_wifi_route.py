"""Exercise DHCP acquisition/renewal against the actual route override module."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FAKES = r'''
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_IPADDR_TYPE_V4 0
#define CONFIG_HOMEHUB_WIFI_CUSTOM_ROUTE 1
#define CONFIG_HOMEHUB_WIFI_GATEWAY "192.168.5.2"
#define CONFIG_HOMEHUB_WIFI_DNS "198.18.0.2"
typedef struct { uint32_t addr; } ip4_addr_t;
typedef struct { struct { union { ip4_addr_t ip4; } u_addr; int type; } ip; } esp_netif_dns_info_t;
typedef enum { ESP_NETIF_DNS_MAIN, ESP_NETIF_DNS_BACKUP, ESP_NETIF_DNS_FALLBACK, ESP_NETIF_DNS_MAX } esp_netif_dns_type_t;
struct netif { ip4_addr_t ip, mask, gw; };
typedef struct { struct netif *impl; uint32_t dns[3]; int dhcp_running; } esp_netif_t;
typedef unsigned netif_nsc_reason_t;
typedef struct { int unused; } netif_ext_callback_args_t;
typedef void (*callback_fn)(struct netif *, netif_nsc_reason_t, const netif_ext_callback_args_t *);
typedef struct { callback_fn fn; } netif_ext_callback_t;
#define LWIP_NSC_IPV4_ADDR_VALID 1
#define LWIP_NSC_IPV4_GATEWAY_CHANGED 2
#define netif_ip4_addr(n) (&(n)->ip)
#define netif_ip4_netmask(n) (&(n)->mask)
#define ip4_addr_isany(a) (!(a)->addr)
#define ip4_addr_ismulticast(a) (((a)->addr >> 28) == 14)
#define ip4_addr_cmp(a,b) ((a)->addr == (b)->addr)
#define ip4_addr_netcmp(a,b,m) (((a)->addr & (m)->addr) == ((b)->addr & (m)->addr))
#define ip4_addr_isbroadcast(a,n) (((a)->addr | (n)->mask.addr) == UINT32_MAX)
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(a) ((a)->addr>>24), (((a)->addr>>16)&255), (((a)->addr>>8)&255), ((a)->addr&255)
#define ESP_LOGI(tag,...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag,...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag,...) do { (void)(tag); } while (0)
static int tcpip_context, dns_calls;
static netif_ext_callback_t *hook;
static int ip4addr_aton(const char *s, ip4_addr_t *out) {
    unsigned a,b,c,d;
    if (sscanf(s,"%u.%u.%u.%u",&a,&b,&c,&d)!=4 || a>255 || b>255 || c>255 || d>255) return 0;
    out->addr=(a<<24)|(b<<16)|(c<<8)|d; return 1;
}
static void *esp_netif_get_netif_impl(esp_netif_t *sta) { return sta->impl; }
static esp_err_t esp_netif_tcpip_exec(esp_err_t (*fn)(void *), void *ctx) {
    tcpip_context=1; int err=fn(ctx); tcpip_context=0; return err;
}
static void netif_add_ext_callback(netif_ext_callback_t *cb, callback_fn fn) {
    assert(tcpip_context); cb->fn=fn; hook=cb;
}
static void netif_set_gw(struct netif *n, const ip4_addr_t *gw) {
    assert(tcpip_context); n->gw=*gw;
    hook->fn(n,LWIP_NSC_IPV4_GATEWAY_CHANGED,NULL);
}
static esp_err_t esp_netif_set_dns_info(esp_netif_t *sta, esp_netif_dns_type_t slot, esp_netif_dns_info_t *dns) {
    assert(tcpip_context); sta->dns[slot]=dns->ip.u_addr.ip4.addr; ++dns_calls; return ESP_OK;
}
'''
HARNESS = r'''
#include "wifi_route.c"
static void lease(struct netif *n) {
    tcpip_context=1; hook->fn(n,LWIP_NSC_IPV4_ADDR_VALID,NULL); tcpip_context=0;
}
int main(void) {
    struct netif n={{0xc0a805b1},{0xffffff00},{0xc0a80501}};
    esp_netif_t sta={.impl=&n,.dns={1,2,3},.dhcp_running=1};
    assert(wifi_route_init(&sta)==ESP_OK);
    lease(&n);
    assert(n.ip.addr==0xc0a805b1 && n.mask.addr==0xffffff00 && sta.dhcp_running);
    assert(n.gw.addr==0xc0a80502 && dns_calls==3);
    for(int i=0;i<3;++i) assert(sta.dns[i]==0xc6120002);
    /* Renewal: DHCP restores its gateway and secondary DNS, same IP. */
    n.gw.addr=0xc0a80501; sta.dns[1]=9; lease(&n);
    assert(n.gw.addr==0xc0a80502 && sta.dns[1]==0xc6120002 && dns_calls==6);
    /* Renewal on an already matching gateway still overrides changed DNS. */
    sta.dns[0]=9; lease(&n); assert(sta.dns[0]==0xc6120002 && dns_calls==9);
    struct netif other={{0xc0a805b2},{0xffffff00},{0xc0a80501}};
    lease(&other); assert(other.gw.addr==0xc0a80501 && dns_calls==9);
    n.ip.addr=0xc0a801b1; n.gw.addr=0xc0a80101; sta.dns[0]=9;
    lease(&n); assert(n.gw.addr==0xc0a80101 && sta.dns[0]==9 && dns_calls==9);
    n.ip.addr=0; lease(&n); assert(dns_calls==9);
    n.ip.addr=0xc0a80502; lease(&n); assert(dns_calls==9);
    puts("DHCP acquisition, renewal, DNS-only renewal, interface isolation, subnet guard, disconnect, and self-gateway guard passed");
    return 0;
}
'''

class WifiRouteTest(unittest.TestCase):
    def test_dhcp_lifecycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / 'fakes.h').write_text(FAKES)
            for name in ['esp_netif.h', 'sdkconfig.h', 'esp_log.h', 'esp_netif_net_stack.h', 'lwip/netif.h']:
                header = path / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "fakes.h"\n')
            (path / 'harness.c').write_text(HARNESS)
            binary = path / ('route.exe' if os.name == 'nt' else 'route')
            command = shlex.split(os.environ.get('CC', 'cc')) + [
                '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(path),
                '-I', str(ROOT / 'main'), str(path / 'harness.c'), '-o', str(binary)]
            subprocess.run(command, check=True, capture_output=True, timeout=30)
            subprocess.run([str(binary)], check=True, capture_output=True, timeout=10)

if __name__ == '__main__':
    unittest.main()
