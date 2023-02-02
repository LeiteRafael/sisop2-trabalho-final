#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DISCOVERY_PORT 9999
#define MAX_MSG_LEN 65536


typedef struct __packet{
    uint16_t type;        // Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn;        // Número de sequência
    uint16_t length;      // Comprimento do payload
    uint16_t timestamp;   // Timestamp do dado
    const char* _payload; // Dados da mensagem
} packet;


int main(int argc, char *argv[]) {
    packet msg_packet;

    int discovery_socket;
    char discovery_buf[MAX_MSG_LEN];
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

    if(strcmp(argv[1],"manager") == 0){
        memset(&discovery_addr, 0, sizeof(discovery_addr));
        discovery_addr.sin_family = AF_INET;
        discovery_addr.sin_port = htons(DISCOVERY_PORT);
        discovery_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

        if (bind(discovery_socket, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr)) < 0) {
            perror("Falhou no binding.");
            exit(EXIT_FAILURE);
        }

        // aguarda a mensagem ser enviada
        while (1) {
            int bytes_received = recvfrom(discovery_socket, discovery_buf, sizeof(discovery_buf), 0, (struct sockaddr *) &discovery_addr, &addr_len);
            if (bytes_received > 0) {
                printf("%s entrou: \"%s\"\n", inet_ntoa(((struct sockaddr_in *) &discovery_addr)->sin_addr), discovery_buf);

                // avisa que recebeu
                char response[] = "Ok! Voce foi conectado.";
                sendto(discovery_socket, response, sizeof(response), 0, (struct sockaddr *) &discovery_addr, sizeof(discovery_addr));
            }
        }

        //close(discovery_socket);
    }
    else{
        char* discovery_msg;
            
        // ip e porta do líder
        memset(&leader_addr, 0, sizeof(leader_addr));
        leader_addr.sin_family = AF_INET;
        leader_addr.sin_port = htons(DISCOVERY_PORT);
        leader_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

        // envia mensagem para o líder
        msg_packet._payload = "Mensagem aqui.";
        sendto(discovery_socket, msg_packet._payload, sizeof(msg_packet), 0, (struct sockaddr *) &leader_addr, sizeof(leader_addr));

        // aqui seria onde vai receber o pacote WoL
        while (1) {
            int bytes_received = recvfrom(discovery_socket, discovery_msg, sizeof(discovery_msg), 0, (struct sockaddr *) &leader_addr, &addr_len_l);

            if (bytes_received > 0) {
                bytes_received = 0;
                printf("%s\n", discovery_msg);
                break;
            }
        }

        //close(discovery_socket);
    }

    close(discovery_socket);

    return 0;
}