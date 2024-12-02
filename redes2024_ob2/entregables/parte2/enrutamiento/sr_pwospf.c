/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 *
 * Descripción:
 * Este archivo contiene las funciones necesarias para el manejo de los paquetes
 * OSPF.
 *
 *---------------------------------------------------------------------------*/

#include "sr_pwospf.h"
#include "sr_router.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "sr_utils.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_rt.h"
#include "pwospf_neighbors.h"
#include "pwospf_topology.h"
#include "dijkstra.h"

/*pthread_t hello_thread;*/
pthread_t g_hello_packet_thread;
pthread_t g_all_lsu_thread;
pthread_t g_lsu_thread;
pthread_t g_neighbors_thread;
pthread_t g_topology_entries_thread;
pthread_t g_rx_lsu_thread;
pthread_t g_dijkstra_thread;

pthread_mutex_t g_dijkstra_mutex = PTHREAD_MUTEX_INITIALIZER;

struct in_addr g_router_id;
uint8_t g_ospf_multicast_mac[ETHER_ADDR_LEN];
struct ospfv2_neighbor *g_neighbors;
struct pwospf_topology_entry *g_topology;
uint16_t g_sequence_num;

/* -- Declaración de hilo principal de la función del subsistema pwospf --- */
static void *pwospf_run_thread(void *arg);

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Configura las estructuras de datos internas para el subsistema pwospf
 * y crea un nuevo hilo para el subsistema pwospf.
 *
 * Se puede asumir que las interfaces han sido creadas e inicializadas
 * en este punto.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance *sr)
{
    assert(sr);

    sr->ospf_subsys = (struct pwospf_subsys *)malloc(sizeof(struct
                                                            pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);

    g_router_id.s_addr = 0;

    /* Defino la MAC de multicast a usar para los paquetes HELLO */
    g_ospf_multicast_mac[0] = 0x01;
    g_ospf_multicast_mac[1] = 0x00;
    g_ospf_multicast_mac[2] = 0x5e;
    g_ospf_multicast_mac[3] = 0x00;
    g_ospf_multicast_mac[4] = 0x00;
    g_ospf_multicast_mac[5] = 0x05;

    g_neighbors = NULL;

    g_sequence_num = 0;

    struct in_addr zero;
    zero.s_addr = 0;
    g_neighbors = create_ospfv2_neighbor(zero);
    g_topology = create_ospfv2_topology_entry(zero, zero, zero, zero, zero, 0);

    /* -- start thread subsystem -- */
    if (pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr))
    {
        perror("pthread_create");
        assert(0);
    }

    return 0; /* success */
} /* -- pwospf_init -- */

/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys *subsys)
{
    if (pthread_mutex_lock(&subsys->lock))
    {
        assert(0);
    }
}

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys *subsys)
{
    if (pthread_mutex_unlock(&subsys->lock))
    {
        assert(0);
    }
}

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Hilo principal del subsistema pwospf.
 *
 *---------------------------------------------------------------------*/

static void *pwospf_run_thread(void *arg)
{
    sleep(5);

    struct sr_instance *sr = (struct sr_instance *)arg;

    /* Set the ID of the router */
    while (g_router_id.s_addr == 0)
    {
        struct sr_if *int_temp = sr->if_list;
        while (int_temp != NULL)
        {
            if (int_temp->ip > g_router_id.s_addr)
            {
                g_router_id.s_addr = int_temp->ip;
            }

            int_temp = int_temp->next;
        }
    }
    Debug("\n\nPWOSPF: Selecting the highest IP address on a router as the router ID\n");
    Debug("-> PWOSPF: The router ID is [%s]\n", inet_ntoa(g_router_id));

    Debug("\nPWOSPF: Detecting the router interfaces and adding their networks to the routing table\n");
    struct sr_if *int_temp = sr->if_list;
    while (int_temp != NULL)
    {
        struct in_addr ip;
        ip.s_addr = int_temp->ip;
        struct in_addr gw;
        gw.s_addr = 0x00000000;
        struct in_addr mask;
        mask.s_addr = int_temp->mask;
        struct in_addr network;
        network.s_addr = ip.s_addr & mask.s_addr;

        if (check_route(sr, network) == 0)
        {
            Debug("-> PWOSPF: Adding the directly connected network [%s, ", inet_ntoa(network));
            Debug("%s] to the routing table\n", inet_ntoa(mask));
            sr_add_rt_entry(sr, network, gw, mask, int_temp->name, 1);
        }
        int_temp = int_temp->next;
    }

    Debug("\n-> PWOSPF: Printing the forwarding table\n");
    sr_print_routing_table(sr);

    pthread_create(&g_hello_packet_thread, NULL, send_hellos, sr);
    pthread_create(&g_all_lsu_thread, NULL, send_all_lsu, sr);
    pthread_create(&g_neighbors_thread, NULL, check_neighbors_life, sr);
    pthread_create(&g_topology_entries_thread, NULL, check_topology_entries_age, sr);

    return NULL;
} /* -- run_ospf_thread -- */

/***********************************************************************************
 * Métodos para el manejo de los paquetes HELLO y LSU
 * SU CÓDIGO DEBERÍA IR AQUÍ
 * *********************************************************************************/

/*---------------------------------------------------------------------
 * Method: check_neighbors_life
 *
 * Chequea si los vecinos están vivos
 *
 *---------------------------------------------------------------------*/

void *check_neighbors_life(void *arg)
{
    Debug("neighbors life check:\n");
    struct sr_instance* sr = (struct sr_instance*)arg;
    while (1){
        usleep(1000000); /*Wait 1 second*/
        struct ospfv2_neighbor* neighborsToDelete = check_neighbors_alive(g_neighbors);
       while (neighborsToDelete != NULL){ /* Recorro todas mis ifaces, con cada vecino a eliminar*/
            struct sr_if* iface = sr->if_list;
            while (iface != NULL){
                if (iface->neighbor_id == neighborsToDelete->neighbor_id.s_addr)
                    iface->neighbor_id = 0; /* Elimino el vecino de la interfaz, no es necesario eliminar su ip y mask */
                Debug("check_neighbors_life: Neighbor %s removed from interface %s\n", inet_ntoa(neighborsToDelete->neighbor_id), iface->name);
                iface = iface->next;
            }
            /* Avanzo los vecinos a eliminar y libero mem*/
            struct ospfv2_neighbor* temp = neighborsToDelete; 
            neighborsToDelete = neighborsToDelete->next;
            free(temp);
        }
    }    
    return NULL;
} /* -- check_neighbors_life -- */

/*---------------------------------------------------------------------
 * Method: check_topology_entries_age
 *
 * Check if the topology entries are alive
 * and if they are not, remove them from the topology table
 *
 *---------------------------------------------------------------------*/

void *check_topology_entries_age(void *arg)
{
    struct sr_instance *sr = (struct sr_instance *)arg;
    dijkstra_param_t *dij_param = (dijkstra_param_t *)malloc(sizeof(dijkstra_param_t));
    while (1)
    {
        /*
        Cada 1 segundo, chequea el tiempo de vida de cada entrada
        de la topologia.
        Si hay un cambio en la topología, se llama a la función de Dijkstra
        en un nuevo hilo.
        Se sugiere también imprimir la topología resultado del chequeo.
        */
        usleep(1000000); /*Wait 1 second*/
        /*print_topolgy_table(g_topology);*/
        if (check_topology_age(g_topology) != 0){
            dij_param->mutex = g_dijkstra_mutex;
            dij_param->topology = g_topology;
            dij_param->rid = g_router_id;
            dij_param->sr = sr;
            pthread_create(&g_dijkstra_thread, NULL, run_dijkstra, dij_param);
            print_topolgy_table(g_topology);
        }
    }


    return NULL;
} /* -- check_topology_entries_age -- */

/*---------------------------------------------------------------------
 * Method: send_hellos
 *
 * Para cada interfaz y cada helloint segundos, construye mensaje
 * HELLO y crea un hilo con la función para enviar el mensaje.
 *
 *---------------------------------------------------------------------*/

void *send_hellos(void *arg)
{
    struct sr_instance *sr = (struct sr_instance *)arg;
    powspf_hello_lsu_param_t *hello_param = (powspf_hello_lsu_param_t *)malloc(sizeof(powspf_hello_lsu_param_t));
    /* While true */
    while (1)
    {
        /* Se ejecuta cada 1 segundo */
        usleep(1000000);
        /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
        /* Chequeo todas las interfaces para enviar el paquete HELLO */
        /* Cada interfaz matiene un contador en segundos para los HELLO*/
        /* Reiniciar el contador de segundos para HELLO */
        /*Debug("Hello check:\n");*/
        struct sr_if *iface = sr->if_list;
        pwospf_lock(sr->ospf_subsys);
        while (iface != NULL)
        {
            if (iface->helloint <= 0){
                hello_param->interface = iface;
                hello_param->sr = sr;
                
                send_hello_packet(hello_param);
                Debug("Hello sent in: %s \n", iface->name);
                iface->helloint = OSPF_DEFAULT_HELLOINT;
                
            }else
                iface->helloint -= 1;
            iface = iface->next;
        }
        pwospf_unlock(sr->ospf_subsys);

        /* Desbloqueo */
    };

    return NULL;
} /* -- send_hellos -- */

/*---------------------------------------------------------------------
 * Method: send_hello_packet
 *
 * Recibe un mensaje HELLO, agrega cabezales y lo envía por la interfaz
 * correspondiente.
 *
 *---------------------------------------------------------------------*/

void *send_hello_packet(void *arg)
{
    powspf_hello_lsu_param_t *hello_param = ((powspf_hello_lsu_param_t *)(arg));

    Debug("\n\nPWOSPF: Constructing HELLO packet for interface %s: \n", hello_param->interface->name);

    uint8_t *hello_packet = (uint8_t *)malloc(sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));

    sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *) hello_packet;
    sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t *)(hello_packet + sizeof(sr_ethernet_hdr_t));
    ospfv2_hdr_t *ospf_hdr = (ospfv2_hdr_t *)(hello_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    ospfv2_hello_hdr_t *hello_hdr = (ospfv2_hello_hdr_t *)(hello_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));

    memcpy(ether_hdr->ether_dhost,(uint8_t *) g_ospf_multicast_mac,sizeof(uint8_t) * ETHER_ADDR_LEN);
    memcpy(ether_hdr->ether_shost,(uint8_t *) hello_param->interface->addr,sizeof(uint8_t) * ETHER_ADDR_LEN);

    /* Seteo el ether_type en el cabezal Ethernet */
    ether_hdr->ether_type = htons(ethertype_ip);

    /* Inicializo cabezal IP */
    /* Seteo el protocolo en el cabezal IP para ser el de OSPF (89) */
    /* Seteo IP origen con la IP de mi interfaz de salida */
    /* Seteo IP destino con la IP de Multicast dada: OSPF_AllSPFRouters  */
    /* Calculo y seteo el chechsum IP*/

    ip_hdr->ip_v = 4;
    ip_hdr->ip_p = ip_protocol_ospfv2;
    ip_hdr->ip_hl = 5;
    ip_hdr->ip_tos = 0;
    ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));
    ip_hdr->ip_id =  0;
    ip_hdr->ip_off = htons(IP_DF);
    ip_hdr->ip_ttl = 64;
    ip_hdr->ip_src = hello_param->interface->ip;
    ip_hdr->ip_dst = htonl(OSPF_AllSPFRouters); 
    ip_hdr->ip_sum = ip_cksum(ip_hdr, sizeof(sr_ip_hdr_t));

    /* Inicializo cabezal de PWOSPF con version 2 y tipo HELLO */
    /* Seteo el Router ID con mi ID*/
    /* Seteo el Area ID en 0 */
    /* Seteo el Authentication Type y Authentication Data en 0*/
    /* Seteo máscara con la máscara de mi interfaz de salida */
    /* Seteo Hello Interval con OSPF_DEFAULT_HELLOINT */
    /* Seteo Padding en 0*/
    ospf_hdr->version = OSPF_V2;
    ospf_hdr->type = OSPF_TYPE_HELLO;
    ospf_hdr->len = htons(sizeof(ospfv2_hello_hdr_t) + sizeof(ospfv2_hdr_t));
    ospf_hdr->rid = g_router_id.s_addr;
    ospf_hdr->aid = 0;
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;

    /* Creo el paquete a transmitir */
    hello_hdr->nmask = hello_param->interface->mask;
    hello_hdr->helloint = htons(OSPF_DEFAULT_HELLOINT);
    hello_hdr->padding = 0;

    /* Calculo y actualizo el checksum del cabezal OSPF */
    ospf_hdr->csum = ospfv2_cksum(ospf_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t)); 

    unsigned int packet_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t);

    struct in_addr ip;
    ip.s_addr = ip_hdr->ip_src;
    struct in_addr mask;
    mask.s_addr = hello_hdr->nmask;
    Debug("-> PWOSPF: Sending HELLO Packet of length = %d, out of the interface: %s\n", packet_len, hello_param->interface->name);
    Debug("      [Router ID = %s]\n", inet_ntoa(g_router_id));
    Debug("      [Router IP = %s]\n", inet_ntoa(ip));
    Debug("      [Network Mask = %s]\n", inet_ntoa(mask));

    /* Envío el paquete HELLO  */
    sr_send_packet(hello_param->sr,(uint8_t *) hello_packet, packet_len, hello_param->interface->name);
    /* Imprimo información del paquete HELLO enviado */
    return NULL;
} /* -- send_hello_packet -- */

/*---------------------------------------------------------------------
 * Method: send_all_lsu
 *
 * Construye y envía LSUs cada 30 segundos
 *
 *---------------------------------------------------------------------*/

void* send_all_lsu(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;
    powspf_hello_lsu_param_t* lsu_param = (powspf_hello_lsu_param_t*)malloc(sizeof(powspf_hello_lsu_param_t));   
    /* while true*/
    while(1)
    {
        /* Se ejecuta cada OSPF_DEFAULT_LSUINT segundos */
        usleep(OSPF_DEFAULT_LSUINT * 1000000);

        /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
        
        pwospf_lock(sr->ospf_subsys);
        /* Recorro todas las interfaces para enviar el paquete LSU */
        struct sr_if* iface = sr->if_list;
        while (iface != NULL){
            if (iface->neighbor_id != 0){ /* Si la interfaz tiene un vecino, envío un LSU */
                lsu_param->interface = iface;
                lsu_param->sr = sr;
                
                send_lsu(lsu_param);
                /* Desbloqueo */
                
            }
            iface = iface->next;
        }
        pwospf_unlock(sr->ospf_subsys);
    };
    return NULL;
} /* -- send_all_lsu -- */


/*---------------------------------------------------------------------
 * Method: send_lsu
 *
 * Construye y envía paquetes LSU a través de una interfaz específica
 *
 *---------------------------------------------------------------------*/

void *send_lsu(void *arg)
{
    powspf_hello_lsu_param_t *lsu_param = ((powspf_hello_lsu_param_t *)(arg));

    /* Solo envío LSUs si del otro lado hay un router*/
    if (lsu_param->interface->neighbor_id == 0){ 
        Debug("No neighbor found, exit\n");
        return NULL;
    }

    /* Construyo el LSU */
    Debug("\n\nPWOSPF: Constructing LSU packet\n");
    unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + (sizeof(ospfv2_lsa_t) * count_routes(lsu_param->sr));
    
    uint8_t *lsu_packet = (uint8_t *)malloc(len);
    sr_ethernet_hdr_t *ether_hdr = (sr_ethernet_hdr_t *) lsu_packet;
    /* Dirección MAC destino la dejo para el final ya que hay que hacer ARP */
    memcpy(ether_hdr->ether_shost,(uint8_t *) lsu_param->interface->addr,sizeof(uint8_t) * ETHER_ADDR_LEN);
    ether_hdr->ether_type = htons(ethertype_ip);

    /* Inicializo cabezal IP*/
    sr_ip_hdr_t * ip_hdr = (sr_ip_hdr_t *)(lsu_packet + sizeof(sr_ethernet_hdr_t));
    ip_hdr->ip_v = 4;
    ip_hdr->ip_p = ip_protocol_ospfv2;
    ip_hdr->ip_hl = 5;
    ip_hdr->ip_tos = 0;
    ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t) * count_routes(lsu_param->sr));
    ip_hdr->ip_id = 0;
    ip_hdr->ip_off = htons(IP_DF);
    ip_hdr->ip_ttl = 64;
    ip_hdr->ip_src = lsu_param->interface->ip;
    ip_hdr->ip_dst = lsu_param->interface->neighbor_ip;    /* La IP destino es la del vecino contectado a mi interfaz*/
    ip_hdr->ip_sum = ip_cksum(ip_hdr, sizeof(sr_ip_hdr_t));

    /* Inicializo cabezal de OSPF*/
    /* Seteo el número de secuencia y avanzo*/
    /* Seteo el TTL en 64 y el resto de los campos del cabezal de LSU */
    /* Seteo el número de anuncios con la cantidad de rutas a enviar. Uso función count_routes */
    /* Creo el paquete y seteo todos los cabezales del paquete a transmitir */
    ospfv2_hdr_t *ospf_hdr = (ospfv2_hdr_t *)(lsu_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    ospf_hdr->version = OSPF_V2;
    ospf_hdr->len = htons(sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t)  + (sizeof(ospfv2_lsa_t) * count_routes(lsu_param->sr)));
    ospf_hdr->rid = g_router_id.s_addr;
    ospf_hdr->aid = 0;
    ospf_hdr->autype = 0;
    ospf_hdr->audata = 0;
    ospf_hdr->type = OSPF_TYPE_LSU;

    ospfv2_lsu_hdr_t *lsu_hdr = (ospfv2_lsu_hdr_t *)(lsu_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    g_sequence_num++;
    lsu_hdr->seq = g_sequence_num;
    lsu_hdr->unused = 0;
    lsu_hdr->ttl = 64;
    lsu_hdr->num_adv = htonl(count_routes(lsu_param->sr));
    /* Creo cada LSA iterando en las enttadas de la tabla */
    /* Solo envío entradas directamente conectadas y agreagadas a mano*/
    /* Creo LSA con subnet, mask y routerID (id del vecino de la interfaz)*/
    ospfv2_lsa_t *lsa = ((ospfv2_lsa_t*)(malloc(sizeof(ospfv2_lsa_t))));
    struct sr_rt *rt = lsu_param->sr->routing_table;
    
    unsigned int i = 0;
    while (rt != NULL && i < ntohl(lsu_hdr->num_adv)){ /* Solo envío entradas directamente conectadas y agregadas a mano, recorro mi tabla hasta encontrar todas las entradas validas*/
        if (rt->admin_dst <= 1){ 
            lsa->subnet = rt->dest.s_addr;
            lsa->mask = rt->mask.s_addr;
            lsa->rid = sr_get_interface(lsu_param->sr, rt->interface)->neighbor_id; 
            memcpy(lsu_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + (sizeof(ospfv2_lsa_t) * i),
                lsa, sizeof(ospfv2_lsa_t));
            i++;
        }
        rt = rt->next;
    }
    free(lsa);
    ospf_hdr->csum = ospfv2_cksum(ospf_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + sizeof(ospfv2_lsa_t) * ntohl(lsu_hdr->num_adv) );       
    
    rt = sr_find_lpm_route(lsu_param->sr->routing_table, ip_hdr->ip_dst); 
    if (rt == NULL){
        Debug("No route found\n");
        return NULL;
    }      
    struct sr_arpentry* arp_entry = sr_arpcache_lookup(&lsu_param->sr->cache, (rt->gw.s_addr != 0) ?  rt->gw.s_addr : ip_hdr->ip_dst );
    if (arp_entry) {
        /* ARP entry found, format the packet and send it */
        Debug("**** -> ARP entry found, format Ethernet header\n");
        memcpy(ether_hdr->ether_dhost, (uint8_t *) arp_entry->mac ,sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set destination MAC address */
        sr_send_packet(lsu_param->sr, lsu_packet, len, lsu_param->interface->name); 
        free(arp_entry);
    } else {
        /* ARP entry not found, queue the query */
        Debug("**** -> ARP entry not found, queue the ARP request (send_LSU)\n");
        struct sr_arpreq *req = sr_arpcache_queuereq(&lsu_param->sr->cache, (rt->gw.s_addr != 0) ?  rt->gw.s_addr : ip_hdr->ip_dst , lsu_packet, len, lsu_param->interface->name);
        handle_arpreq(lsu_param->sr, req);
    }
    return NULL;
} /* -- send_lsu -- */

/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_hello_packet
 *
 * Gestiona los paquetes HELLO recibidos
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_hello_packet(struct sr_instance *sr, uint8_t *packet, unsigned int length, struct sr_if *rx_if){
    ospfv2_hello_hdr_t *rx_hello_hdr = (ospfv2_hello_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    ospfv2_hdr_t *rx_ospfv2_hdr = (ospfv2_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    sr_ip_hdr_t *rx_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));

    struct in_addr neighbor_id;
    struct in_addr neighbor_ip;
    struct in_addr net_mask;
    /* obtener info del hello que me llega como */
    neighbor_id.s_addr = rx_ospfv2_hdr->rid;
    neighbor_ip.s_addr = rx_ip_hdr->ip_src; 
    net_mask.s_addr = rx_hello_hdr->nmask;
    /* Imprimo info del paquete recibido*/
    Debug("->HANDLING_HELLO_PWOSPF: Detecting PWOSPF HELLO Packet :\n");
    Debug("      [Neighbor ID = %s]\n", inet_ntoa(neighbor_id));
    Debug("      [Neighbor IP = %s]\n", inet_ntoa(neighbor_ip));
    Debug("      [Network Mask = %s]\n", inet_ntoa(net_mask));

    /* Chequeo checksum */
    if (rx_ospfv2_hdr->csum != ospfv2_cksum(rx_ospfv2_hdr, length - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t))){
        /*
        Debug("Checksum received: %d\n", rx_ospfv2_hdr->csum);
        Debug("Checksum calculation: %d\n", ospfv2_cksum(rx_ospfv2_hdr, length - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t)));*/
        Debug("-> PWOSPF: HELLO Packet dropped, invalid checksum\n");
        return;
    }
    /* Chequeo de la máscara de red */
    if (rx_hello_hdr->nmask != rx_if->mask){
        Debug("-> PWOSPF: HELLO Packet dropped, invalid hello network mask\n");
        return;
    }
    /* Chequeo si el intervalo de HELLO es el correcto */
    if (rx_hello_hdr->helloint != htons(OSPF_DEFAULT_HELLOINT)){
        Debug("-> PWOSPF: HELLO Packet dropped, invalid hello interval\n");
        return;
    }
    /* Seteo el vecino en la interfaz por donde llegó y actualizo la lista de vecinos */
    char new_neighbor = 0;
    if (rx_if->neighbor_id != rx_ospfv2_hdr->rid){
        Debug("New neighbor found\n");
        rx_if->neighbor_id = rx_ospfv2_hdr->rid;
        new_neighbor = 1;
    }
    rx_if->neighbor_ip = neighbor_ip.s_addr;
    /* Recorro todas las interfaces para enviar el paquete LSU */
    /* Si la interfaz tiene un vecino, envío un LSU */
    refresh_neighbors_alive(g_neighbors, neighbor_id); /* tmb añade los vecinos y esas cosas que hace si se lee el codigo */
    powspf_hello_lsu_param_t* lsu_param = (powspf_hello_lsu_param_t *)malloc(sizeof(powspf_hello_lsu_param_t));;

    if (new_neighbor){ /* Si es un nuevo vecino, debo enviar LSUs por todas mis interfaces*/
        struct sr_if *iface = sr->if_list;
        while (iface != NULL) {
            if (iface->neighbor_id != 0 && iface != rx_if){ /* Si la interfaz tiene un vecino, envío un LSU */
                lsu_param->interface = iface;
                lsu_param->sr = sr;
                Debug("\n\nPWOSPF: Constructing LSU packet (update new neighbor)\n");
                send_lsu(lsu_param);
            }          
            iface = iface->next;
        }        
    }
} /* -- sr_handle_pwospf_hello_packet -- */

/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_lsu_packet
 *
 * Gestiona los paquetes LSU recibidos y actualiza la tabla de topología
 * y ejecuta el algoritmo de Dijkstra
 *
 *---------------------------------------------------------------------*/

void *sr_handle_pwospf_lsu_packet(void *arg) {
    powspf_rx_lsu_param_t *rx_lsu_param = ((powspf_rx_lsu_param_t *)(arg));
    
    sr_ip_hdr_t *rx_ip_hdr = (sr_ip_hdr_t *)(rx_lsu_param->packet + sizeof(sr_ethernet_hdr_t));
    ospfv2_hdr_t *rx_ospfv2_hdr = (ospfv2_hdr_t *)(rx_lsu_param->packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    ospfv2_lsu_hdr_t *rx_lsu_hdr = (ospfv2_lsu_hdr_t *)(rx_lsu_param->packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    ospfv2_lsa_t *rx_lsa = (ospfv2_lsa_t *)(rx_lsu_param->packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t));


    struct in_addr next_hop_id;
    struct in_addr next_hop_ip;
    next_hop_id.s_addr = rx_ospfv2_hdr->rid;
    next_hop_ip.s_addr = rx_ip_hdr->ip_src;

    /* Obtengo el Router ID del router originario del LSU y chequeo si no es mío*/
    if (rx_ospfv2_hdr->rid == g_router_id.s_addr) {
        Debug("-> PWOSPF: LSU Packet dropped, originated by this router\n");
        return NULL;
    }

    if (rx_lsu_hdr->ttl <= 1) {
        return NULL;
    }

    /* Chequeo checksum (Lo hace en is_valid_packet, igualmente funciona esta implementacion)*/ 
    uint16_t calc_checksum = ospfv2_cksum(rx_ospfv2_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + (sizeof(ospfv2_lsa_t) * ntohl(rx_lsu_hdr->num_adv)));
    if (rx_ospfv2_hdr->csum != calc_checksum) {
        Debug("-> PWOSPF: LSU Packet dropped, invalid checksum\n");
        return NULL;
    }

    /* Obtengo el número de secuencia y uso check_sequence_number para ver si ya lo recibí desde ese vecino*/
    if (check_sequence_number(g_topology, next_hop_id , rx_lsu_hdr->seq) == 0) {
        Debug("-> PWOSPF: LSU Packet dropped, repeated sequence number\n");
        return NULL;
    }
    /* Obtengo el vecino que me envió el LSU*/


    /* Imprimo info del paquete recibido*/
    
    Debug("-> PWOSPF: Detecting LSU Packet from [Neighbor ID = %s, IP = %s]\n", inet_ntoa(next_hop_id), inet_ntoa(next_hop_ip));

    unsigned int len = rx_lsu_param->length;
    uint8_t *packet = (uint8_t *) malloc(len);

    memcpy(packet, rx_lsu_param->packet, len);

    sr_ethernet_hdr_t *tx_ether_hdr = (sr_ethernet_hdr_t *)(packet);
    sr_ip_hdr_t *tx_ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
    ospfv2_hdr_t *tx_ospfv2_hdr = (ospfv2_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    ospfv2_lsu_hdr_t *tx_lsu_hdr = (ospfv2_lsu_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
    tx_lsu_hdr->ttl -= 1;

    /* Itero en los LSA que forman parte del LSU. Para cada uno, actualizo la topología.*/
    Debug("-> PWOSPF: Processing LSAs and updating topology table\n");

    int i;
    for ( i = 0; i < ntohl(rx_lsu_hdr->num_adv); i++) {
        rx_lsa = (ospfv2_lsa_t *)(rx_lsu_param->packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) +
            sizeof(ospfv2_lsu_hdr_t) + (sizeof(ospfv2_lsa_t) * i));
        struct in_addr net_num = {rx_lsa->subnet};
        struct in_addr net_mask = {rx_lsa->mask};
        struct in_addr neighbor_id = {rx_lsa->rid};

        Debug("      [Subnet = %s]", inet_ntoa(net_num));
        Debug("      [Mask = %s]", inet_ntoa(net_mask));
        Debug("      [Neighbor ID = %s]\n", inet_ntoa(neighbor_id));
        /* LLamo a refresh_topology_entry*/
        refresh_topology_entry(g_topology, next_hop_id, net_num, net_mask, neighbor_id, next_hop_ip, rx_lsu_hdr->seq);
    }
    /* Imprimo la topología */
    Debug("\n-> PWOSPF: Printing the topology table\n");
    print_topolgy_table(g_topology);

    /* Ejecuto Dijkstra en un nuevo hilo (run_dijkstra)*/
    dijkstra_param_t *dij_param = (dijkstra_param_t *)malloc(sizeof(dijkstra_param_t));
    dij_param->mutex = g_dijkstra_mutex;
    dij_param->topology = g_topology;
    dij_param->rid = g_router_id;
    dij_param->sr = rx_lsu_param->sr;
    pthread_create(&g_dijkstra_thread, NULL,run_dijkstra , dij_param);

       
    /* Flooding del LSU por todas las interfaces menos por donde me llegó */
            /* Seteo MAC de origen */
            /* Ajusto paquete IP, origen y checksum*/
            /* Ajusto cabezal OSPF: checksum y TTL*/
            /* Envío el paquete*/

    struct sr_if *iface = rx_lsu_param->sr->if_list;
    while (iface != NULL) {
        if (iface->neighbor_id != 0 && iface != rx_lsu_param->rx_if) {
            memcpy(tx_ether_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);

            tx_ip_hdr->ip_src = iface->ip;
            tx_ip_hdr->ip_dst = iface->neighbor_ip;
            tx_ip_hdr->ip_sum = ip_cksum(tx_ip_hdr, sizeof(sr_ip_hdr_t));

            tx_ospfv2_hdr->csum = ospfv2_cksum(tx_ospfv2_hdr, sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + (sizeof(ospfv2_lsa_t) * ntohl(tx_lsu_hdr->num_adv)));
            struct sr_rt* rt = sr_find_lpm_route(rx_lsu_param->sr->routing_table, tx_ip_hdr->ip_dst); 
            if (rt == NULL){
                Debug("No route found\n");
                return NULL;
            }      
            struct sr_arpentry* arp_entry = sr_arpcache_lookup(&rx_lsu_param->sr->cache, (rt->gw.s_addr != 0) ?  rt->gw.s_addr : tx_ip_hdr->ip_dst );
            Debug("*** -> Get interface\n");
            if (arp_entry) {
                /* ARP entry found, format the packet and send it */
                Debug("**** -> ARP entry found, format Ethernet header\n");
                memcpy(tx_ether_hdr->ether_dhost, (uint8_t *) arp_entry->mac ,sizeof(uint8_t) * ETHER_ADDR_LEN); /* Set destination MAC address */
                sr_send_packet(rx_lsu_param->sr, packet, len, iface->name); 
                Debug("**** -> Packet sent (send_LSU)\n");
                free(arp_entry);
            } else {
                /* ARP entry not found, queue the query */
                Debug("**** -> ARP entry not found, queue the ARP request (send_LSU)\n");
                struct sr_arpreq *req = sr_arpcache_queuereq(&rx_lsu_param->sr->cache, (rt->gw.s_addr != 0) ?  rt->gw.s_addr : tx_ip_hdr->ip_dst , packet, len, iface->name);
                handle_arpreq(rx_lsu_param->sr, req);
                Debug("**** -> Packet not sent, must process queue (send_LSU)\n");
            }
            Debug("-> PWOSPF: Sending LSU Packet of length = %d, out of the interface: %s\n", rx_lsu_param->length, iface->name);
        }
        iface = iface->next;
    }    
    return NULL;
} /* -- sr_handle_pwospf_lsu_packet -- */



/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_packet
 *
 * Gestiona los paquetes PWOSPF
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_packet(struct sr_instance *sr, uint8_t *packet, unsigned int length, struct sr_if *rx_if)
{
    /*Si aún no terminó la inicialización, se descarta el paquete recibido*/
    if (g_router_id.s_addr == 0) {
       return;
    }

    ospfv2_hdr_t *rx_ospfv2_hdr = ((ospfv2_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
    powspf_rx_lsu_param_t *rx_lsu_param = ((powspf_rx_lsu_param_t *)(malloc(sizeof(powspf_rx_lsu_param_t))));

    Debug("-> PWOSPF: Detecting PWOSPF Packet\n");
    Debug("      [Type = %d]\n", rx_ospfv2_hdr->type);

    switch (rx_ospfv2_hdr->type)
    {
    case OSPF_TYPE_HELLO:
        sr_handle_pwospf_hello_packet(sr, packet, length, rx_if);
        break;
    case OSPF_TYPE_LSU:
        rx_lsu_param->sr = sr;
        unsigned int i;
        for (i = 0; i < length; i++)
        {
            rx_lsu_param->packet[i] = packet[i];
        }
        rx_lsu_param->length = length;
        rx_lsu_param->rx_if = rx_if;
        
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_t pid;
        pthread_create(&pid, &attr, sr_handle_pwospf_lsu_packet, rx_lsu_param);
        break;
    }
} /* -- sr_handle_pwospf_packet -- */
