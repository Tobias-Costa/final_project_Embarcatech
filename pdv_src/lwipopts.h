#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                      1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0

#define LWIP_NETIF_HOSTNAME         1
#define LWIP_TIMEVAL_PRIVATE        0
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Memória e Malloc da SDK
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    16000

// Funcionalidades de Rede
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_MQTT                   1

#define MQTT_OUTPUT_RINGBUF_SIZE    1024

#endif
