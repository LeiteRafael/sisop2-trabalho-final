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


/* ================================ */
/* ==== ESQUELETOS DAS FUNÇÕES ==== */
/* ================================ */
char* get_mac_address();
char* get_hostname();