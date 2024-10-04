#ifndef __ESP_TCP_H__
#define __ESP_TCP_H__

#include "stdio.h"
#include "esp_common.h"
#include "uart.h"
#include "lwip\netifapi.h"
#include "lwip\lwip\tcp.h"

err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);

err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
void tcp_err_callback(void *arg, err_t err);
err_t tcp_poll_callback(void *arg, struct tcp_pcb *tpcb);

#endif