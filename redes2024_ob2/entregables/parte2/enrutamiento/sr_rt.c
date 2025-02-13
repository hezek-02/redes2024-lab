/*-----------------------------------------------------------------------------
 * file:  sr_rt.c
 * date:  Mon Oct 07 04:02:12 PDT 2002
 * Author:  casado@stanford.edu
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>


#include <sys/socket.h>
#include <netinet/in.h>
#define __USE_MISC 1 /* force linux to show inet_aton */
#include <arpa/inet.h>

#include "sr_rt.h"
#include "sr_router.h"

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

int sr_load_rt(struct sr_instance* sr,const char* filename)
{
    FILE* fp;
    char  line[BUFSIZ];
    char  dest[32];
    char  gw[32];
    char  mask[32];
    char  iface[32];
    struct in_addr dest_addr;
    struct in_addr gw_addr;
    struct in_addr mask_addr;
    int clear_routing_table = 0;

    /* -- REQUIRES -- */
    assert(filename);
    if( access(filename,R_OK) != 0)
    {
        perror("access");
        return -1;
    }

    fp = fopen(filename,"r");

    while( fgets(line,BUFSIZ,fp) != 0)
    {
        sscanf(line,"%s %s %s %s",dest,gw,mask,iface);
        if(inet_aton(dest,&dest_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    dest);
            return -1; 
        }
        if(inet_aton(gw,&gw_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    gw);
            return -1; 
        }
        if(inet_aton(mask,&mask_addr) == 0)
        { 
            fprintf(stderr,
                    "Error loading routing table, cannot convert %s to valid IP\n",
                    mask);
            return -1; 
        }
        if( clear_routing_table == 0 ){
            printf("Loading routing table from server, clear local routing table.\n");
            sr->routing_table = 0;
            clear_routing_table = 1;
        }
        sr_add_rt_entry(sr,dest_addr,gw_addr,mask_addr,iface, 0);
    } /* -- while -- */

    return 0; /* -- success -- */
} /* -- sr_load_rt -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

void sr_add_rt_entry(struct sr_instance* sr, struct in_addr dest,
struct in_addr gw, struct in_addr mask, char* if_name, uint8_t admin_dst)
{
    struct sr_rt* rt_walker = 0;

    /* -- REQUIRES -- */
    assert(if_name);
    assert(sr);

    /* -- empty list special case -- */
    if(sr->routing_table == 0)
    {
        sr->routing_table = (struct sr_rt*)malloc(sizeof(struct sr_rt));
        assert(sr->routing_table);
        sr->routing_table->next = 0;
        sr->routing_table->dest = dest;
        sr->routing_table->gw   = gw;
        sr->routing_table->mask = mask;
        strncpy(sr->routing_table->interface,if_name,sr_IFACE_NAMELEN);
        sr->routing_table->admin_dst = admin_dst;

        return;
    }

    /* -- find the end of the list -- */
    rt_walker = sr->routing_table;
    while(rt_walker->next){
      rt_walker = rt_walker->next; 
    }

    rt_walker->next = (struct sr_rt*)malloc(sizeof(struct sr_rt));
    assert(rt_walker->next);
    rt_walker = rt_walker->next;

    rt_walker->next = 0;
    rt_walker->dest = dest;
    rt_walker->gw   = gw;
    rt_walker->mask = mask;
    strncpy(rt_walker->interface,if_name,sr_IFACE_NAMELEN);
    rt_walker->admin_dst = admin_dst;

} /* -- sr_add_entry -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

void sr_print_routing_table(struct sr_instance* sr)
{
    struct sr_rt* rt_walker = 0;

    if(sr->routing_table == 0)
    {
        printf(" *warning* Routing table empty \n");
        return;
    }

    printf("-----------------------------------------------------------------------\n");
    printf("%-18s%-18s%-18s%-8sAdmin Dis\n", "Destination", "Gateway", "Subnet Mask", "Iface");
    printf("-----------------------------------------------------------------------\n");

    rt_walker = sr->routing_table;
    
    sr_print_routing_entry(rt_walker);
    while(rt_walker->next)
    {
        rt_walker = rt_walker->next; 
        sr_print_routing_entry(rt_walker);
    }

} /* -- sr_print_routing_table -- */

/*---------------------------------------------------------------------
 * Method:
 *
 *---------------------------------------------------------------------*/

void sr_print_routing_entry(struct sr_rt* entry)
{
    /* -- REQUIRES --*/
    assert(entry);
    assert(entry->interface);

    printf("%-18s",inet_ntoa(entry->dest));
    printf("%-18s",inet_ntoa(entry->gw));
    printf("%-18s",inet_ntoa(entry->mask));
    printf("%-8s",entry->interface);
    printf("%d\n",entry->admin_dst);

} /* -- sr_print_routing_entry -- */

/*---------------------------------------------------------------------
 * Method: count_routes
 *
 * Counting directly connected and static routes in the routing table 
 *
 *---------------------------------------------------------------------*/

int count_routes(struct sr_instance* sr)
{
    int count = 0;
    struct sr_rt* entry = sr->routing_table;
    while(entry != NULL)
    {
        if (entry->admin_dst <= 1)
        {
            count++;
        }
        entry = entry->next;
    }
    return count;
} /* -- count_routes -- */

/*---------------------------------------------------------------------
 * Method: clean_routes
 *
 * Clean the dynamic routes from the routing table 
 *
 *---------------------------------------------------------------------*/

void clear_routes(struct sr_instance* sr)
{
    struct sr_rt* entry = sr->routing_table;
    while(entry/*->next*/ != NULL)
    {
/*printf("entry: %s\t", inet_ntoa(entry->dest));*/
        if (entry->next == NULL)
        {
            break;
        }
/*printf("entry->next: %s\n", inet_ntoa(entry->next->dest));*/
        if (entry->next->admin_dst > 1)
        {
            sr_del_rt_entry(entry);
        }
        else
        {
            entry = entry->next;
        }
    }
} /* -- clean_routes -- */

/*---------------------------------------------------------------------
 * Method: sr_del_rt_entry
 *
 * Delete route
 *
 *---------------------------------------------------------------------*/

void sr_del_rt_entry(struct sr_rt* previous_entry)
{
    struct sr_rt* temp = previous_entry->next;

    if (previous_entry->next->next != NULL)
    {
        previous_entry->next = previous_entry->next->next;
    }
    else
    {
        previous_entry->next = NULL;
    }

    free(temp);
} /* -- sr_del_rt_entry -- */

/*---------------------------------------------------------------------
 * Method: check_route
 *
 * Check route existance
 *
 *---------------------------------------------------------------------*/

uint8_t check_route(struct sr_instance* sr, struct in_addr route)
{
    struct sr_rt* entry = sr->routing_table;
    while(entry != NULL)
    {
        if (entry->dest.s_addr == route.s_addr)
        {
            return 1;
        }

        entry = entry->next;
    }

    return 0;
} /* -- check_route -- */

/*---------------------------------------------------------------------
 * Method: sr_find_lpm_route
 * Scope:  Global
 *
 * This function finds the longest prefix match route for a given destination IP.
 *
 * Parameters:
 *   rt - pointer to the routing table
 *   ip_dest - destination IP address
 *
 * Returns:
 *   Pointer to the longest prefix match route, or NULL if no match is found
 *---------------------------------------------------------------------*/
struct sr_rt* sr_find_lpm_route(struct sr_rt* rt, uint32_t ip_dest)
{
    struct sr_rt* rt_walker = rt;
    struct sr_rt* lpm_route = 0;
    while (rt_walker)
    {
        uint32_t masked_dest = rt_walker->dest.s_addr & rt_walker->mask.s_addr;
        uint32_t masked_ip_dest = ip_dest & rt_walker->mask.s_addr;

        if (masked_dest == masked_ip_dest)
        {
            if (lpm_route == 0 || rt_walker->mask.s_addr > lpm_route->mask.s_addr)
                lpm_route = rt_walker;         
        }

        rt_walker = rt_walker->next;
    }
    return lpm_route;
}
