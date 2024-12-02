/**********************************************************************
 * file:  sr_router.c
 *
 * Descripción:
 *
 * Este archivo contiene todas las funciones que interactúan directamente
 * con la tabla de enrutamiento, así como el método de entrada principal
 * para el enrutamiento.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "pwospf_protocol.h"
#include "sr_pwospf.h"

uint8_t sr_multicast_mac[ETHER_ADDR_LEN];

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Inicializa el subsistema de enrutamiento
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    assert(sr);

    /* Inicializa el subsistema OSPF */
    pwospf_init(sr);

    /* Dirección MAC de multicast OSPF */
    sr_multicast_mac[0] = 0x01;
    sr_multicast_mac[1] = 0x00;
    sr_multicast_mac[2] = 0x5e;
    sr_multicast_mac[3] = 0x00;
    sr_multicast_mac[4] = 0x00;
    sr_multicast_mac[5] = 0x05;

    /* Inicializa la caché y el hilo de limpieza de la caché */
    sr_arpcache_init(&(sr->cache));

    /* Inicializa los atributos del hilo */
    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    /* Hilo para gestionar el timeout del caché ARP */
    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

} /* -- sr_init -- */

/* Envía un paquete ICMP de error */
void sr_send_icmp_error_packet(uint8_t type,
                              uint8_t code,
                              struct sr_instance *sr,
                              uint32_t new_ip_src,
                              uint8_t *ipPacket
                              ){
    unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
    /*Crear un nuevo paquete para el mensaje de error ICMP1*/
    uint8_t *icmp_error_packet = malloc(len);
    memset(icmp_error_packet, 0, len);
    /*Obtener el encabezado IP original del paquete que causó el error*/
    sr_ip_hdr_t *original_ip_hdr = (sr_ip_hdr_t *)(ipPacket + sizeof(sr_ethernet_hdr_t));
    /*Obtener encabezados para el paquete de error*/
    sr_ethernet_hdr_t *icmp_error_eth_hdr = (sr_ethernet_hdr_t *) icmp_error_packet;
    sr_icmp_t3_hdr_t *icmp_error_icmp_hdr = (sr_icmp_t3_hdr_t *)(icmp_error_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    sr_ip_hdr_t *icmp_error_ip_hdr = (sr_ip_hdr_t *)(icmp_error_packet + sizeof(sr_ethernet_hdr_t));

    /*Config IP hdr*/
    memcpy(icmp_error_packet, ipPacket, len);
    icmp_error_ip_hdr->ip_ttl = 64;                      /*TTL inicial*/
    icmp_error_ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));  /*Longitud total del paquete*/
    icmp_error_ip_hdr->ip_p = ip_protocol_icmp;          /*Protocolo ICMP*/
    icmp_error_ip_hdr->ip_src = new_ip_src;  /*IP de origen (del router)*/
    icmp_error_ip_hdr->ip_dst = original_ip_hdr->ip_src;                   /*IP destino (IP del remitente del paquete original)*/
    icmp_error_ip_hdr->ip_sum = ip_cksum(icmp_error_ip_hdr, sizeof(sr_ip_hdr_t));  /*Calcular checksum del encabezado IP*/
    
    /*Config ICMP hdr*/
    memcpy(icmp_error_icmp_hdr->data, original_ip_hdr, ICMP_DATA_SIZE);
    icmp_error_icmp_hdr->icmp_type = type;  /*Tipo de error ICMP*/
    icmp_error_icmp_hdr->icmp_code = code;  /*Código de error ICMP*/
    icmp_error_icmp_hdr->unused = 0;        /*Campo sin usar*/  
    icmp_error_icmp_hdr->next_mtu = 0;      /*MTU del siguiente salto*/
    icmp_error_icmp_hdr->icmp_sum = icmp3_cksum(icmp_error_icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
    
    printf("*** -> Execute LPM in routing table (in icmp send)\n");
    struct sr_rt* rt_entry = sr_find_lpm_route(sr->routing_table, icmp_error_ip_hdr->ip_dst);
    if (rt_entry == 0) {
      printf("**** ->Error cannot send packet\n");
      return;
    }
    printf("*** -> HEADER (icmp) \n"); /* eth mac dont setup yet*/
    print_hdrs(icmp_error_packet, len);
    struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, (rt_entry->gw.s_addr == 0) ? icmp_error_ip_hdr->ip_dst : rt_entry->gw.s_addr);
    printf("*** -> Get interface for send icmp%s\n", rt_entry->interface);
    struct sr_if* interface = sr_get_interface(sr, rt_entry->interface); /* Get the interface */
    if (arp_entry) {
      /* ARP entry found, format the packet and send it */
      printf("**** -> ARP entry found, format Ethernet header for icmp\n");
      memcpy(icmp_error_eth_hdr->ether_dhost, (uint8_t *) arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set destination MAC address */
      memcpy(icmp_error_eth_hdr->ether_shost, (uint8_t *) interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set source MAC address */              
      sr_send_packet(sr, icmp_error_packet, len, interface->name); 
      printf("**** -> Packet sent (icmp)\n");
      free(arp_entry);
    } else {
      printf("**** -> ARP entry not found, queue the ARP request (icmp)\n");
      struct sr_arpreq *req = sr_arpcache_queuereq(&sr->cache, (rt_entry->gw.s_addr == 0) ? icmp_error_ip_hdr->ip_dst : rt_entry->gw.s_addr, icmp_error_packet, len, interface->name);
      handle_arpreq(sr, req);
      printf("**** -> Packet not sent, must process queue(icmp)\n");
    }
    free(icmp_error_packet);
}

void sr_send_icmp_reply(struct sr_instance* sr, uint8_t *packet, unsigned int len){
    /*Crear un nuevo paquete para el mensaje de error ICMP1*/
    printf("*** -> Create ICMP reply packet\n");
    /* Memory setup*/
    uint8_t *icmp_reply_packet = malloc(len);
    memset(icmp_reply_packet, 0, len);
    /*Obtener el encabezado IP original del paquete que causó el error*/
    sr_ip_hdr_t *original_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
    /*Tomar encabezado  IP/ethernet  para el paquete de error*/
    sr_ethernet_hdr_t *icmp_reply_eth_hdr = (sr_ethernet_hdr_t *) icmp_reply_packet;
    sr_ip_hdr_t *icmp_reply_ip_hdr = (sr_ip_hdr_t *)(icmp_reply_packet + sizeof(sr_ethernet_hdr_t));
    sr_icmp_hdr_t *icmp_reply_icmp_hdr = (sr_icmp_hdr_t *)(icmp_reply_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    /*Config*/
    memcpy(icmp_reply_packet, packet, len);
    icmp_reply_ip_hdr->ip_ttl = 64;                      /*TTL inicial*/
    icmp_reply_ip_hdr->ip_p = ip_protocol_icmp;          /*Protocolo ICMP*/
    icmp_reply_ip_hdr->ip_src = original_ip_hdr->ip_dst;  /*IP de origen (del router)*/
    icmp_reply_ip_hdr->ip_dst = original_ip_hdr->ip_src;  /*IP destino (IP del remitente del paquete original)*/
    icmp_reply_ip_hdr->ip_sum = ip_cksum(icmp_reply_ip_hdr, sizeof(sr_ip_hdr_t));  /*Calcular checksum del encabezado IP*/
    /*Crear encabezado ICMP de tipo 0 (echo Reply)*/
    icmp_reply_icmp_hdr->icmp_type = 0;  
    icmp_reply_icmp_hdr->icmp_code = 0;  
    icmp_reply_icmp_hdr->icmp_sum = icmp_cksum(icmp_reply_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t)); /*Calcular checksum del encabezado ICMP*/
    struct sr_rt* rt_entry = sr_find_lpm_route(sr->routing_table, icmp_reply_ip_hdr->ip_dst);
    if (rt_entry == 0) {
      printf("**** ->Error cannot send packet\n");
      return;
    }
    printf("*** -> HEADER (icmp) \n"); /* eth mac dont setup yet*/
    print_hdrs(icmp_reply_packet, len);
    struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, (rt_entry->gw.s_addr == 0) ? icmp_reply_ip_hdr->ip_dst : rt_entry->gw.s_addr);
    printf("*** -> Get interface for send icmp %s\n", rt_entry->interface);
    struct sr_if* interface = sr_get_interface(sr, rt_entry->interface); /* Get the interface */
    if (arp_entry) {
      /* ARP entry found, format the packet and send it */
      printf("**** -> ARP entry found, format Ethernet header for icmp\n");
      memcpy(icmp_reply_eth_hdr->ether_dhost, (uint8_t *) arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set destination MAC address */
      memcpy(icmp_reply_eth_hdr->ether_shost, (uint8_t *) interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set source MAC address */
      sr_send_packet(sr, icmp_reply_packet, len, interface->name); 
      printf("**** -> Packet sent (icmp)\n");
      free(arp_entry);
    } else {
      printf("**** -> ARP entry not found, queue the ARP request (icmp)\n");
      struct sr_arpreq *req = sr_arpcache_queuereq(&sr->cache, (rt_entry->gw.s_addr == 0) ? icmp_reply_ip_hdr->ip_dst : rt_entry->gw.s_addr, icmp_reply_packet, len, interface->name);
      handle_arpreq(sr, req);
      printf("**** -> Packet not sent, must process queue (icmp)\n");
    }
    free(icmp_reply_packet);
}

void sr_handle_ip_packet(struct sr_instance *sr,
        uint8_t *packet /* lent */,
        unsigned int len,
        uint8_t *srcAddr,
        uint8_t *destAddr,
        char *interface /* lent */,
        sr_ethernet_hdr_t *eHdr) {
  printf("* -> Handle ip packet ON.\n");
 /*  printf("ROUTING TABLE: \n");
  sr_print_routing_table(sr); */
  print_hdrs(packet,len); /* Print packet headers */
  /* Get IP datagram */
  if (eHdr->ether_type == htons(ethertype_ip)) { 
    sr_ip_hdr_t *ipHdr = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t)); /* Cast packet to IP header */

    /* Verify checksum of the IP header */
    if (ip_cksum(ipHdr, sizeof(sr_ip_hdr_t)) == ipHdr->ip_sum){ 
      /* Query routing table */
      if (ipHdr->ip_p == ip_protocol_ospfv2 && ipHdr->ip_dst == htonl(OSPF_AllSPFRouters)){
        printf("****-> It is an PWOSPF packet (Multicast, hello):\n");
        print_hdr_ospf(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
        struct sr_if* myIface = sr_get_interface(sr, interface);
        sr_handle_pwospf_packet(sr, packet, len, myIface);
        return;
      }
      struct sr_if* myIface = sr_get_interface_given_ip(sr, ipHdr->ip_dst);
      if (myIface != 0) { /* Packet is for me */
        printf("**** -> Packet is for one of my interfaces.\n");
        if (ipHdr->ip_p == ip_protocol_icmp) {
          printf("***** -> It is an ICMP packet.\n");
          sr_icmp_hdr_t *icmpHdr = (sr_icmp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)); /* Cast packet to ICMP header */
          if (icmpHdr->icmp_type == 8) { /* ICMP echo request */
            printf("****** -> ICMP echo request, generate ICMP echo reply.\n");
            sr_send_icmp_reply(sr, packet, len);
          }
        }else if (ipHdr->ip_p == ip_protocol_ospfv2){
          printf("***** -> It is an PWOSPF packet to one of my ifaces. (LSU)\n");
          print_hdr_ospf(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
          sr_handle_pwospf_packet(sr, packet, len, myIface);
        }else {
          printf("***** -> It is an TCP/UDP packet. Need to send ICMP type 3, code 3. Port unreachable.\n");
          struct sr_if* rx_if = sr_get_interface(sr, interface); /* Get the interface */
          sr_send_icmp_error_packet(3,3,sr,rx_if->ip,packet);
        }
        return;
      }
      
      if (ipHdr->ip_p == ip_protocol_icmp ){ 
        int cumulative_sz = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);
        sr_icmp_hdr_t *icmpHdr = (sr_icmp_hdr_t *) (packet + cumulative_sz);
        if ((icmpHdr->icmp_type == 8 || icmpHdr->icmp_type == 3)  && (icmp_cksum(icmpHdr, len - cumulative_sz) != icmpHdr->icmp_sum)) 
          return;
        printf("**** -> It is an ICMP packet and checksum are OK.\n");
      }
      printf("*** -> Execute LPM in routing table\n");
      struct sr_rt* rt_entry = sr_find_lpm_route(sr->routing_table, ipHdr->ip_dst);
      if (rt_entry == 0) {
        /* Send ICMP HOST unreachable */
        printf("**** -> There is no matching entry in the routing table. Need to send ICMP type 3, code 1. HOST unreachable\n");
        struct sr_if* myInterface = sr_get_interface(sr, interface); /* Get the interface */
        uint32_t ip_src = myInterface->ip; /* Get the source IP */
        sr_send_icmp_error_packet(3, 1, sr, ip_src, packet);
        return;
      }
      printf("*** -> Decrement TTL and recalculate checksum: TTL, %d\n", ipHdr->ip_ttl);
      ipHdr->ip_ttl--; /* Decrement TTL */
      ipHdr->ip_sum = ip_cksum(ipHdr, sizeof(sr_ip_hdr_t)); /* Recalculate checksum */
      if (ipHdr->ip_ttl == 0) {
        /* Send ICMP time exceeded */
        printf("**** -> Need to send ICMP type 11, time exceeded\n");
        struct sr_if* myInterface = sr_get_interface(sr, interface); /* Get the interface */
        uint32_t ip_src = myInterface->ip; /* Get the source IP */
        sr_send_icmp_error_packet(11, 0, sr, ip_src, packet);
        return;
      }
      /* Destination reachable, now use ARP to send the packet */
      printf("*** -> Get ARP entry\n");
      struct sr_arpentry* arp_entry = sr_arpcache_lookup(&sr->cache, (rt_entry->gw.s_addr == 0) ? ipHdr->ip_dst : rt_entry->gw.s_addr);
      printf("*** -> Get interface\n");
      struct sr_if* interface = sr_get_interface(sr, rt_entry->interface); /* Get the interface */
      if (arp_entry) {
        /* ARP entry found, format the packet and send it */
        printf("**** -> ARP entry found, format Ethernet header\n");
        memcpy(eHdr->ether_dhost, (uint8_t *) arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set destination MAC address */
        memcpy(eHdr->ether_shost, (uint8_t *) interface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set source MAC address */              
        sr_send_packet(sr, packet, len, interface->name); 
        printf("**** -> Packet sent, forwarding success\n");
        free(arp_entry);
      } else {
        /* ARP entry not found, queue the query */
        printf("**** -> ARP entry not found, queue the ARP request\n");
        struct sr_arpreq *req = sr_arpcache_queuereq(&sr->cache, (rt_entry->gw.s_addr == 0) ? ipHdr->ip_dst : rt_entry->gw.s_addr, packet, len, interface->name);
        handle_arpreq(sr, req);
        printf("**** -> Packet not sent, must process queue\n");
      }
      rt_entry = NULL;
      interface = NULL;
    } else 
      printf("Checksum is incorrect\n");
  } else 
    printf("Packet is not IP\n");
  printf("Finished packet handler iteration\n");
}

/* Envía todos los paquetes IP pendientes de una solicitud ARP */
void sr_arp_reply_send_pending_packets(struct sr_instance *sr,
                                        struct sr_arpreq *arpReq,
                                        uint8_t *dhost,
                                        uint8_t *shost,
                                        struct sr_if *iface) {

  struct sr_packet *currPacket = arpReq->packets;
  sr_ethernet_hdr_t *ethHdr;
  uint8_t *copyPacket;

  while (currPacket != NULL) {
     ethHdr = (sr_ethernet_hdr_t *) currPacket->buf;
     memcpy(ethHdr->ether_shost, dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
     memcpy(ethHdr->ether_dhost, shost, sizeof(uint8_t) * ETHER_ADDR_LEN);

     copyPacket = malloc(sizeof(uint8_t) * currPacket->len);
     memcpy(copyPacket, ethHdr, sizeof(uint8_t) * currPacket->len);

     print_hdrs(copyPacket, currPacket->len);
     sr_send_packet(sr, copyPacket, currPacket->len, iface->name);
     currPacket = currPacket->next;
  }
}

/* Gestiona la llegada de un paquete ARP*/
void sr_handle_arp_packet(struct sr_instance *sr,
        uint8_t *packet /* lent */,
        unsigned int len,
        uint8_t *srcAddr,
        uint8_t *destAddr,
        char *interface /* lent */,
        sr_ethernet_hdr_t *eHdr) {

  /* Imprimo el cabezal ARP */
  printf("*** -> It is an ARP packet. Print ARP header.\n");
  print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo el cabezal ARP */
  sr_arp_hdr_t *arpHdr = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo las direcciones MAC */
  unsigned char senderHardAddr[ETHER_ADDR_LEN], targetHardAddr[ETHER_ADDR_LEN];
  memcpy(senderHardAddr, arpHdr->ar_sha, ETHER_ADDR_LEN);
  memcpy(targetHardAddr, arpHdr->ar_tha, ETHER_ADDR_LEN);

  /* Obtengo las direcciones IP */
  uint32_t senderIP = arpHdr->ar_sip;
  uint32_t targetIP = arpHdr->ar_tip;
  unsigned short op = ntohs(arpHdr->ar_op);

  /* Verifico si el paquete ARP es para una de mis interfaces */
  struct sr_if *myInterface = sr_get_interface_given_ip(sr, targetIP);

  if (op == arp_op_request) {  /* Si es un request ARP */
    printf("**** -> It is an ARP request.\n");

    /* Si el ARP request es para una de mis interfaces */
    if (myInterface != 0) {
      printf("***** -> ARP request is for one of my interfaces.\n");

      /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
      printf("****** -> Add MAC->IP mapping of sender to my ARP cache.\n");
      sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);

      /* Construyo un ARP reply y lo envío de vuelta */
      printf("****** -> Construct an ARP reply and send it back.\n");
      memcpy(eHdr->ether_shost, (uint8_t *) myInterface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(eHdr->ether_dhost, (uint8_t *) senderHardAddr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_sha, myInterface->addr, ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_tha, senderHardAddr, ETHER_ADDR_LEN);
      arpHdr->ar_sip = targetIP;
      arpHdr->ar_tip = senderIP;
      arpHdr->ar_op = htons(arp_op_reply);

      /* Imprimo el cabezal del ARP reply creado */
      print_hdrs(packet, len);

      sr_send_packet(sr, packet, len, myInterface->name);
    }

    printf("******* -> ARP request processing complete.\n");

  } else if (op == arp_op_reply) {  /* Si es un reply ARP */

    printf("**** -> It is an ARP reply.\n");

    /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
    printf("***** -> Add MAC->IP mapping of sender to my ARP cache.\n");
    struct sr_arpreq *arpReq = sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);
    
    if (arpReq != NULL) { /* Si hay paquetes pendientes */

    	printf("****** -> Send outstanding packets.\n");
    	sr_arp_reply_send_pending_packets(sr, arpReq, (uint8_t *) myInterface->addr, (uint8_t *) senderHardAddr, myInterface);
    	sr_arpreq_destroy(&(sr->cache), arpReq);

    }
    printf("******* -> ARP reply processing complete.\n");
  }
}

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* Obtengo direcciones MAC origen y destino */
  sr_ethernet_hdr_t *eHdr = (sr_ethernet_hdr_t *) packet;
  uint8_t *destAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint8_t *srcAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(destAddr, eHdr->ether_dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(srcAddr, eHdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint16_t pktType = ntohs(eHdr->ether_type);

  if (is_packet_valid(packet, len)) {
    if (pktType == ethertype_arp) {
      sr_handle_arp_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    } else if (pktType == ethertype_ip) {
      sr_handle_ip_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    }
  }

}/* end sr_ForwardPacket */