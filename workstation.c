/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "manager.h"
#include "guest.h"


/* =========================== */
/* ==== VARIÁVEIS GLOBAIS ==== */
/* =========================== */
int isManager = 0;


/* ============== */
/* ==== MAIN ==== */
/* ============== */
int main(int argc, char** argv) {
    system("clear");
    
    // 1. Aguarda o recebimento de uma mensagem via broadcast do atual manager, 
    // que irá dizer se já existe um manager. Esse "recvfrom()" deverá possuir um timeout de 2 segundos.

    // cria e configura socket para se comunicar com o "manager"
    int discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("[Discovery|Manager] Erro ao criar o socket");
        exit(1);
    }

    // habilita modo broadcast
    int broadcast_enable = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("[Discovery|Manager] Erro ao habilitar o modo Broadcast");
        exit(1);
    }

    // habilita reuse port
    int reuse_port_enable = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &reuse_port_enable, sizeof(reuse_port_enable)) < 0) {
        perror("[Discovery|Manager] Erro ao habilitar o modo Reuse Port");
        exit(1);
    }

    // habilita timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("[Monitoring|Guest] Erro ao habilitar o modo Timeout");
        exit(1);
    }

    struct sockaddr_in discovery_addr;
    memset(&discovery_addr, 0, sizeof(discovery_addr));
    discovery_addr.sin_family = AF_INET;
    discovery_addr.sin_port = htons(DISCOVERY_PORT);
    discovery_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // binding
    if (bind(discovery_socket, (struct sockaddr*)&discovery_addr, sizeof(discovery_addr)) < 0) {
        perror("[Discovery|Manager] Erro ao realizar o binding");
        exit(1);
    }

    char buffer[MAX_MSG_LEN];
    struct sockaddr_in guest_addr;
    socklen_t guest_addr_len = sizeof(guest_addr);
    // aguarda recebimento de mensagem, indicando que ja há um manager
    ssize_t recv_len = recvfrom(discovery_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&guest_addr, &guest_addr_len);
    if (recv_len >= 0) {
        // 2.1. Caso receba uma mensagem, significa que a máquina em questão é o guest,
        // e deverá retornar um "ack" (passa seu hostname e mac address) ao manager, indicando que o reconhece como manager.
        isManager = 0;

        // envia uma mensagem para o manager contendo seu mac, hostname
        // coleta mac address e hostname
        char *mac = get_mac_address();
        char *hostname = get_hostname();

        // coloca mac e hostname em uma string para enviar ao "manager"
        char *message = (char *) malloc(strlen(mac) + strlen(hostname) + 2);
        sprintf(message, "%s,%s", mac, hostname);

        if (sendto(discovery_socket, message, strlen(message), 0, (struct sockaddr*)&discovery_addr, sizeof(discovery_addr)) < 0) {
            perror("[Discovery|Guest] Erro ao enviar mensagem ao manager (JOIN_REQUEST)");
            exit(1);
        }

        free(mac);
        free(hostname);
        free(message);
    }
    else {
        // 2.2. Caso não receba uma mensagem, significa que a máquina em questão é o manager.
        isManager = 1;
    }

    do{
        while(!isManager){
            // 1. Não faz nada e apenas aguarda até que, se for o caso,
            // ele seja eleito o novo manager pelo algoritmo de eleição.
        }

        while(isManager){
            // 1. Recebe o "ack" do guest, contendo o hostname e o mac address dele, e insere o guest na tabela.

            // 2. Envia uma mensagem em broadcast, indicando que é o manager.
        }
    }

    return 0;
}