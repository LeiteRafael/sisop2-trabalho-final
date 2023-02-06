#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>

#define DISCOVERY_PORT 9999
#define MAX_MSG_LEN 256
#define BROADCAST_IP "255.255.255.255"

typedef struct __packet{
    uint16_t type;        // Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn;        // Número de sequência
    uint16_t length;      // Comprimento do payload
    uint16_t timestamp;   // Timestamp do dado
    const char* _payload; // Dados da mensagem
} packet;


// serviço de monitoramento, parte do manager (envia pacotes do tipo sleep status request)
// está executando antes de "conhecer" o guest (erro nas threads)
void *managerMonitoringService(void *socket) {
    int monitoring_socket = *(int*) socket;
    char monitoring_buf[MAX_MSG_LEN] = "sleep status request";
    struct sockaddr_in monitoring_addr;
    socklen_t addr_len = sizeof(monitoring_addr);

    memset(&monitoring_addr, 0, sizeof(monitoring_addr));
    monitoring_addr.sin_family = AF_INET;
    monitoring_addr.sin_port = htons(DISCOVERY_PORT);
    monitoring_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("entra manager\n");
    printf("ip: %d:%d\n", htonl(INADDR_ANY), htons(DISCOVERY_PORT));

    while (1) {
        sendto(monitoring_socket, monitoring_buf, sizeof(monitoring_buf), 0, (struct sockaddr *) &monitoring_addr, sizeof(monitoring_addr));
        sleep(5);

        // ainda falta ler a resposta do guest
    }

    return NULL;
}

// serviço de monitoramento, parte do manager (envia pacotes do tipo sleep status request)
// não está recebendo o sleep status request do manager
void *guestMonitoringService(void *socket) {
    int monitoring_socket = *(int *) socket;
    char monitoring_buf[MAX_MSG_LEN];
    char monitoring_msg[MAX_MSG_LEN];
    struct sockaddr_in monitoring_addr;
    socklen_t addr_len = sizeof(monitoring_addr);

    printf("entra guest");

    while (1) {
        int bytes_received = recvfrom(monitoring_socket, monitoring_buf, sizeof(monitoring_buf), 0, (struct sockaddr *) &monitoring_addr, &addr_len);
        if (bytes_received > 0) {
            printf("Recebeu do Manager: %s\n", monitoring_buf);

            if (strcmp(monitoring_buf, "sleep status request") == 0) {
                char status[] = "Status: awaken";
                sendto(monitoring_socket, status, sizeof(status), 0, (struct sockaddr *) &monitoring_addr, sizeof(monitoring_addr));
            }
        }
    }

    return NULL;
}

// função que será chamada com a criação da thread de descoberta de guests
void *discoveryService(void *socket) {
    int discovery_socket = *(int*) socket;
    char discovery_buf[MAX_MSG_LEN];
    struct sockaddr_in discovery_addr;
    socklen_t addr_len = sizeof(discovery_addr);

    while (1) {
        int bytes_received = recvfrom(discovery_socket, discovery_buf, sizeof(discovery_buf), 0, (struct sockaddr *) &discovery_addr, &addr_len);
        if (bytes_received > 0) {
            printf("%s:%d entrou: \"%s\"\n", inet_ntoa(((struct sockaddr_in *) &discovery_addr)->sin_addr), ntohs(discovery_addr.sin_port), discovery_buf);

            char response[] = "Ok! Voce foi conectado.";
            sendto(discovery_socket, response, sizeof(response), 0, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr));
        }
    }

    return NULL;
}

// pega o MAC address do guest
char* getMacAddress() {
    int sockfd;
    struct ifreq ifr;
    char *iface = "enp0s3"; // mudar aqui para a rede utilizada
    unsigned char *mac;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        return NULL;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        close(sockfd);
        return NULL;
    }

    close(sockfd);

    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    char *mac_str = (char*)malloc(sizeof(char) * 18);
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return mac_str;
}

int main(int argc, char *argv[]) {
    packet msg_packet;

    pthread_t discovery_thread;
    pthread_t manager_monitoring_thread, guest_monitoring_thread;

    int discovery_socket;
    int monitoring_socket;

    char discovery_msg[MAX_MSG_LEN];
    struct sockaddr_in discovery_addr;
    socklen_t addr_len = sizeof(discovery_addr);

    if ((discovery_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Falhou na criacao do socket (Discovery Socket)");
        exit(EXIT_FAILURE);
    }

    if ((monitoring_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Falhou na criacao do socket (Monitoring Socket)");
        exit(EXIT_FAILURE);
    }

    int broadcastEnable = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    memset(&discovery_addr, 0, sizeof(discovery_addr));
    discovery_addr.sin_family = AF_INET;
    discovery_addr.sin_port = htons(DISCOVERY_PORT);
    discovery_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    if(strcmp(argv[1], "manager") == 0){
        if (bind(discovery_socket, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr)) < 0) {
            perror("Falhou no binding");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&discovery_thread, NULL, discoveryService, (void*) &discovery_socket) != 0) {
            perror("Falhou na criacao da thread (Discovery Service)");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&manager_monitoring_thread, NULL, managerMonitoringService, (void*) &monitoring_socket) != 0) {
            perror("Falhou na criacao da thread (Manager Monitoring Service)");
            exit(EXIT_FAILURE);
        }

        pthread_join(discovery_thread, NULL);
        pthread_join(manager_monitoring_thread, NULL);
    }
    else if(strcmp(argv[1], "guest") == 0){
        msg_packet._payload = getMacAddress();

        sendto(discovery_socket, msg_packet._payload, sizeof(msg_packet._payload)*3, 0, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr));

        // aqui seria onde vai receber o pacote WoL
        while (1) {
            int bytes_received = recvfrom(discovery_socket, discovery_msg, MAX_MSG_LEN, 0, (struct sockaddr *) &discovery_addr, &addr_len);

            if (bytes_received > 0) {
                printf("%s\n", discovery_msg);
                break;
            }
        }

        if (pthread_create(&guest_monitoring_thread, NULL, guestMonitoringService, (void*) &monitoring_socket) != 0) {
            perror("Falhou na criacao da thread (Guest Monitoring Service)");
            exit(EXIT_FAILURE);
        }

        pthread_join(guest_monitoring_thread, NULL);
    }

    close(discovery_socket);
    close(monitoring_socket);

    return 0;
}