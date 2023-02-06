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


typedef struct __packet{
    uint16_t type;        // Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn;        // Número de sequência
    uint16_t length;      // Comprimento do payload
    uint16_t timestamp;   // Timestamp do dado
    const char* _payload; // Dados da mensagem
} packet;


char* getMacAddress() {
    int sockfd;
    struct ifreq ifr;
    char *iface = "enp0s3";
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
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return mac_str;
}

int main(int argc, char *argv[]) {
    packet msg_packet;

    int discovery_socket;
    char discovery_buf[MAX_MSG_LEN];
    char discovery_msg[MAX_MSG_LEN];
    struct sockaddr_in discovery_addr;
    socklen_t addr_len = sizeof(discovery_addr);

    struct sockaddr_in leader_addr;
    socklen_t addr_len_l = sizeof(leader_addr);

    if ((discovery_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Falhou na criacao do socket.");
        exit(EXIT_FAILURE);
    }

    int broadcastEnable = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    memset(&discovery_addr, 0, sizeof(discovery_addr));
    discovery_addr.sin_family = AF_INET;
    discovery_addr.sin_port = htons(DISCOVERY_PORT);
    discovery_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    if(strcmp(argv[1], "manager") == 0){
        if (bind(discovery_socket, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr)) < 0) {
            perror("Falhou no binding.");
            exit(EXIT_FAILURE);
        }

        // aguarda a mensagem ser enviada
        while (1) {
            int bytes_received = recvfrom(discovery_socket, discovery_buf, sizeof(discovery_buf), 0, (struct sockaddr *) &discovery_addr, &addr_len);
            if (bytes_received > 0) {
                printf("%s:%d entrou: \"%s\"\n", inet_ntoa(((struct sockaddr_in *) &discovery_addr)->sin_addr), ntohs(discovery_addr.sin_port), discovery_buf);

                // avisa que recebeu
                char response[] = "Ok! Voce foi conectado.";
                sendto(discovery_socket, response, sizeof(response), 0, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr));
            }
        }
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
    }

    close(discovery_socket);

    return 0;
}