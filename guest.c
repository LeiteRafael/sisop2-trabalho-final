/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "guest.h"


/* =========================== */
/* ==== VARIÁVEIS GLOBAIS ==== */
/* =========================== */
extern pthread_mutex_t guest_list_mutex;
char* manager_ip;
int current_guest_id;


/* =================================== */
/* ==== IMPLEMENTAÇÃO DAS FUNÇÕES ==== */
/* =================================== */
void send_discovery_message () {
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
}

void join_monitoring_service () {
    int local_guest_id = current_guest_id;

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

    // usa a porta específica do guest para se comunicar com o serviço de monitoramento
    struct sockaddr_in monitoring_addr;
    memset(&monitoring_addr, 0, sizeof(monitoring_addr));
    monitoring_addr.sin_family = AF_INET;
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

void* guest_interface_service() {
    // identificação se o usuário é "manager" ou "guest"
    char* user_type = "guest";

    printf("[Interface] Servico de Gerenciamento de Sono\n\n");
    printf("[Interface] Comandos disponiveis:\n");
    printf("[Interface] exit                   - Encerra sua participacao no servico.\n");

    while (1) {
        printf("[Interface] Entre com um comando: ");

        char input[50];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            continue;
        }
        input[strcspn(input, "\n")] = 0;

        // separa o comando do parâmetro passado
        char* command = strtok(input, " ");

        if (strcmp(command, "exit") == 0) {
            printf("\n[Interface] Encerrando sua participacao no servico.\n\n");

            // altera o id do guest para -1 para sinalizar para o loop de envio/recebimento
            // de mensagens (na "main") que o guest deve enviar uma mensagem do tipo SLEEP_SERVICE_QUIT
            // para o manager utilizando o monitoring_socket
            current_guest_id = -1;

            // sincronização de condição:
            // aguarda receber mensagem do manager como um "ack" de sua saida do serviço
            while (current_guest_id == -1) {}

            // destrói o mutex e encerra o programa
            pthread_mutex_destroy(&guest_list_mutex);
            exit(0);
        }
        else {
            printf("\n[Interface] Comando invalido.\n\n");
        }
    }
}

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