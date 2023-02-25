/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "guest.h"


/* =================================== */
/* ==== IMPLEMENTAÇÃO DAS FUNÇÕES ==== */
/* =================================== */
char* get_mac_address() {
    // obtém o endereço mac do guest usando chamadas de sistema e regex
    char mac_addr[18];

    FILE *fp = popen("ifconfig | \
                      grep 'HWaddr ' | \
                      awk '{print $5}'", "r");
    if (fp == NULL) {
        perror("[Discovery|Guest] Erro ao obter o endereço MAC ('HWaddr')");
        exit(1);
    }

    fgets(mac_addr, sizeof(mac_addr), fp);
    if(strlen(mac_addr) < 17){
        fp = popen("ifconfig | \
                    grep 'ether ' | \
                    awk '{print $2}'", "r");
        if (fp == NULL) {
            perror("[Discovery|Guest] Erro ao obter o endereço MAC ('ether')");
            exit(1);
        }

        fgets(mac_addr, sizeof(mac_addr), fp);
    }

    pclose(fp);

    char* mac = (char*) malloc(strlen(mac_addr) + 1);
    strcpy(mac, mac_addr);

    return mac;
}

char* get_hostname() {
    // obtém o nome do hostname do guest usando chamadas de sistema e regex
    char hostname[256];

    FILE *fp = popen("hostname", "r");
    if (fp == NULL) {
        perror("[Discovery|Guest] Erro ao obter o hostname");
        exit(1);
    }

    fgets(hostname, sizeof(hostname), fp);
    pclose(fp);

    char* hn = (char*) malloc(strlen(hostname) + 1);
    strcpy(hn, hostname);

    return hn;
}