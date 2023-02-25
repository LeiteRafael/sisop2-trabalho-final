/* ===================== */
/* ==== BIBLIOTECAS ==== */
/* ===================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


/* ==================== */
/* ==== DEFINIÇÕES ==== */
/* ==================== */
#define DISCOVERY_PORT 8000
#define MONITORING_PORT 9000
#define MAX_MSG_LEN 1024
#define MAX_GUESTS_ALLOWED 5


/* ==================== */
/* ==== ESTRUTURAS ==== */
/* ==================== */
struct guest_info {
    char hostname[20*MAX_GUESTS_ALLOWED];
    char ip[16*MAX_GUESTS_ALLOWED];
    char mac_address[18*MAX_GUESTS_ALLOWED];
    char status[10*MAX_GUESTS_ALLOWED];

    int id;
    int port;

    struct guest_info* next;
};


/* ================================ */
/* ==== ESQUELETOS DAS FUNÇÕES ==== */
/* ================================ */
void* discovery_service(void* arg);
void* monitoring_service(void* arg);
void  management_service(char* ip, int port, char* mac, char* hostname, char* buffer);
void* interface_service(void* arg);

void  remove_guest(int id);
void  update_all_guest_ids();
void  send_wol_packet(char* hostname, int id);
void  show_guest_list();