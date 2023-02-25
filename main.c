/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "manager.h"
#include "guest.h"


/* =========================== */
/* ==== VARIÁVEIS GLOBAIS ==== */
/* =========================== */
char* manager_ip;
int current_guest_id;


/* ============== */
/* ==== MAIN ==== */
/* ============== */
int main(int argc, char** argv) {
    system("clear");
    
    if (argc != 2) {
        printf("Uso: %s [manager|guest]\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "manager") == 0) {
        // "manager" cria threads para os serviços de descoberta e monitoramento
        pthread_t discovery_thread, monitoring_thread, interface_thread;

        if (pthread_create(&discovery_thread, NULL, discovery_service, NULL) != 0) {
            perror("[Discovery|Manager] Erro ao criar thread");
            exit(1);
        }

        if (pthread_create(&monitoring_thread, NULL, monitoring_service, NULL) != 0) {
            perror("[Monitoring|Manager] Erro ao criar thread");
            exit(1);
        }

        if (pthread_create(&interface_thread, NULL, interface_service, (void*) argv[1]) != 0) {
            perror("[Interface|Manager] Erro ao criar thread");
            exit(1);
        }

        pthread_join(discovery_thread, NULL);
        pthread_join(monitoring_thread, NULL);
        pthread_join(interface_thread, NULL);
    }
    else if (strcmp(argv[1], "guest") == 0) {
        // cria e configura socket para se comunicar com "manager"
        int discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (discovery_socket < 0) {
            perror("[Discovery|Guest] Erro ao criar o socket");
            exit(1);
        }

        // modo broadcast
        int broadcast_enable = 1;
        if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
            perror("[Discovery|Guest] Erro ao habilitar o modo Broadcast");
            exit(1);
        }

        struct sockaddr_in discovery_addr;
        memset(&discovery_addr, 0, sizeof(discovery_addr));
        discovery_addr.sin_family = AF_INET;
        discovery_addr.sin_port = htons(DISCOVERY_PORT);
        discovery_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        // coleta mac address e nome do hostname
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

        char buffer1[MAX_MSG_LEN];

        // receber id do guest pelo "manager"
        struct sockaddr_in manager_discovery_addr;
        socklen_t manager_discovery_addr_len = sizeof(manager_discovery_addr);
        ssize_t recv_len = recvfrom(discovery_socket, buffer1, sizeof(buffer1), 0, (struct sockaddr*)&manager_discovery_addr, &manager_discovery_addr_len);
        if (recv_len < 0) {
            perror("[Monitoring|Guest] Erro ao receber mensagem do manager (SLEEP_STATUS_REQUEST)");
            exit(1);
        }

        printf("[Discovery|Guest] Comunicacao estabelecida com \"manager\" %s:%d\n", inet_ntoa(manager_discovery_addr.sin_addr), ntohs(manager_discovery_addr.sin_port));

        manager_ip = inet_ntoa(manager_discovery_addr.sin_addr);
        current_guest_id = atoi(buffer1);
        int local_guest_id = current_guest_id;
        printf("[Discovery|Guest] Seu id no servico: %d\n\n", current_guest_id);

        close(discovery_socket);

        // cria e configura socket para se comunicar com "manager"
        int monitoring_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (monitoring_socket < 0) {
            perror("[Monitoring|Guest] Erro ao criar o socket");
            exit(1);
        }

        int enable = 1;
        if (setsockopt(monitoring_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            perror("[Monitoring|Guest] Erro ao habilitar o modo Padrão");
            exit(1);
        }

        struct sockaddr_in monitoring_addr;
        memset(&monitoring_addr, 0, sizeof(monitoring_addr));
        monitoring_addr.sin_family = AF_INET;

        pthread_t interface_thread;
        if (pthread_create(&interface_thread, NULL, interface_service, (void *) argv[1]) != 0) {
            perror("[Interface|Guest] Erro ao criar thread");
            exit(1);
        }

        // usa a porta específica do guest para se comunicar com o serviço de monitoramento
        monitoring_addr.sin_port = htons(MONITORING_PORT + current_guest_id);
        monitoring_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        // binding
        if (bind(monitoring_socket, (struct sockaddr*)&monitoring_addr, sizeof(monitoring_addr)) < 0) {
            perror("[Monitoring|Guest] Erro ao realizar o binding");
            exit(1);
        }

        char buffer[MAX_MSG_LEN];

        while (1) {
            // receber mensagem do "manager"
            struct sockaddr_in manager_addr;
            socklen_t manager_addr_len = sizeof(manager_addr);
            ssize_t recv_len = recvfrom(monitoring_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&manager_addr, &manager_addr_len);
            if (recv_len < 0) {
                perror("[Monitoring|Guest] Erro ao receber mensagem do manager");
                continue;
            }

            buffer[recv_len] = '\0';

            // verifica se é mensagem do tipo SLEEP_STATUS_REQUEST
            if (strcmp(buffer, "SLEEP_STATUS_REQUEST") == 0) {
                if (current_guest_id != -1) {
                    // envia SLEEP_STATUS_RESPONSE
                    char response[MAX_MSG_LEN];
                    snprintf(response, MAX_MSG_LEN, "%s", "awaken");

                    ssize_t send_len = sendto(monitoring_socket, response, strlen(response), 0, (struct sockaddr*)&manager_addr, manager_addr_len);
                    if (send_len < 0) {
                        perror("[Monitoring|Guest] Erro ao enviar mensagem ao manager (SLEEP_STATUS_RESPONSE)");
                        continue;
                    }
                }
                else {
                    // envia SLEEP_SERVICE_QUIT
                    char response[MAX_MSG_LEN];
                    snprintf(response, MAX_MSG_LEN, "SLEEP_SERVICE_QUIT");

                    ssize_t send_len = sendto(monitoring_socket, response, strlen(response), 0, (struct sockaddr*)&manager_addr, manager_addr_len);
                    if (send_len < 0) {
                        perror("[Monitoring|Guest] Erro ao enviar mensagem ao manager (SLEEP_SERVICE_QUIT)");
                        continue;
                    }

                    // aguarda SLEEP_QUIT_ACKNOWLEDGE
                    ssize_t recv_len = recvfrom(monitoring_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&manager_addr, &manager_addr_len);
                    if (recv_len < 0) {
                        perror("[Monitoring|Guest] Erro ao receber mensagem do manager (SLEEP_STATUS_REQUEST)");
                        continue;
                    }
                    
                    if (strcmp(buffer, "SLEEP_QUIT_ACKNOWLEDGE") == 0) {
                        current_guest_id = local_guest_id;
                    }
                }
            }
        }

        close(monitoring_socket);
    }

    return 0;
}