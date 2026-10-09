#pragma once

#include "esp_netif.h"

/* Retain DHCP addressing while overriding the station's gateway and DNS. */
esp_err_t wifi_route_init(esp_netif_t *sta);
