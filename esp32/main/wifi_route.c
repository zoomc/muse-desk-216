#include "wifi_route.h"

#include "sdkconfig.h"

#if CONFIG_HOMEHUB_WIFI_CUSTOM_ROUTE
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/netif.h"

static const char *TAG = "link.wifi.route";
static esp_netif_t *s_sta;
static struct netif *s_netif;
static ip4_addr_t s_gateway;
static esp_netif_dns_info_t s_dns;
static netif_ext_callback_t s_callback;

/* lwIP calls this on its TCP/IP thread, after DHCP applies an address,
 * including renewals with an unchanged IP. netif_set_gw emits only
 * GATEWAY_CHANGED, so it cannot re-enter the ADDR_VALID branch below. */
static void on_address(struct netif *netif, netif_nsc_reason_t reason,
                       const netif_ext_callback_args_t *args)
{
    (void)args;
    if (netif != s_netif || !(reason & LWIP_NSC_IPV4_ADDR_VALID)) return;

    const ip4_addr_t *ip = netif_ip4_addr(netif);
    const ip4_addr_t *mask = netif_ip4_netmask(netif);
    if (ip4_addr_isany(ip) || ip4_addr_isany(mask) ||
        !ip4_addr_netcmp(ip, &s_gateway, mask) ||
        ip4_addr_cmp(ip, &s_gateway) ||
        ip4_addr_isbroadcast(&s_gateway, netif)) {
        ESP_LOGW(TAG, "custom gateway outside usable station subnet; keeping DHCP route");
        return;
    }

    netif_set_gw(netif, &s_gateway);
    /* Override all slots: a DHCP secondary DNS must not bypass the proxy. */
    for (int i = ESP_NETIF_DNS_MAIN; i < ESP_NETIF_DNS_MAX; ++i) {
        esp_err_t err = esp_netif_set_dns_info(s_sta, (esp_netif_dns_type_t)i, &s_dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "DNS override failed: %s", esp_err_to_name(err));
            return;
        }
    }
    ESP_LOGI(TAG, "DHCP IP " IPSTR ", gateway " IPSTR ", DNS " IPSTR,
             IP2STR(ip), IP2STR(&s_gateway), IP2STR(&s_dns.ip.u_addr.ip4));
}

static esp_err_t install_callback(void *ctx)
{
    s_sta = ctx;
    s_netif = esp_netif_get_netif_impl(s_sta);
    if (!s_netif) return ESP_ERR_INVALID_STATE;
    if (!ip4addr_aton(CONFIG_HOMEHUB_WIFI_GATEWAY, &s_gateway) ||
        ip4_addr_isany(&s_gateway) || ip4_addr_ismulticast(&s_gateway)) {
        return ESP_ERR_INVALID_ARG;
    }
    ip4_addr_t dns;
    if (!ip4addr_aton(CONFIG_HOMEHUB_WIFI_DNS, &dns) ||
        ip4_addr_isany(&dns) || ip4_addr_ismulticast(&dns)) {
        return ESP_ERR_INVALID_ARG;
    }
    s_dns.ip.type = ESP_IPADDR_TYPE_V4;
    s_dns.ip.u_addr.ip4.addr = dns.addr;
    netif_add_ext_callback(&s_callback, on_address);
    return ESP_OK;
}
#endif

esp_err_t wifi_route_init(esp_netif_t *sta)
{
#if CONFIG_HOMEHUB_WIFI_CUSTOM_ROUTE
    return esp_netif_tcpip_exec(install_callback, sta);
#else
    (void)sta;
    return ESP_OK;
#endif
}
