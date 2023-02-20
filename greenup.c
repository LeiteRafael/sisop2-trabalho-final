/* =================== */
/* ==== TODO LIST ==== */
/* =================== */
// em ordem de importância:
//    1. fazer o sistema de monitoramento funcionar para mais de um guest
//        - **possível tentativa** usar mais de uma porta MONITORING_PORT
//    2. implementar o envio do pacote WoL
//        - aparentemente fácil, tem na internet
//    3. inserir o serviço de interface em algum lugar
//        - talvez irá precisar criar nova thread
//    4. testar o programa nos labs

// opcional:
//    5. melhorar a documentação
//    6. transformar os "buffers" de envio de mensagem na estrutura citada no pdf
//    7. modularizar melhor o código
//        - separar o código em mais de um arquivo
//        - desmembrar as funções em funções menores
//    8. remover as mensagens desnecessárias
//        - deixar apenas o serviço de interface no console
//    9. modificar a função da lista de guests
//        - imprimir uma mensagem quando a lista estiver vazia


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
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


/* ==================== */
/* ==== DEFINIÇÕES ==== */
/* ==================== */
#define DISCOVERY_PORT 8000
#define MONITORING_PORT 9000
#define MAX_GUESTS 10
#define MAX_MSG_LEN 1024


/* ==================== */
/* ==== ESTRUTURAS ==== */
/* ==================== */
struct guest_info {
    char* hostname;
    char ip[16];
    int port;
    char* status;
    char mac_address[18];
    char network_interface[10];

    struct guest_info* next;
};


/* ================================ */
/* ==== ESQUELETOS DAS FUNÇÕES ==== */
/* ================================ */
void* discovery_service(void* arg);
void* monitoring_service(void* arg);
void  management_service(char* ip, int port, char* mac, char* interface);
void  interface_service();

void  show_guest_list();
char* get_mac_address();
char* get_network_interface();


/* =========================== */
/* ==== VARIÁVEIS GLOBAIS ==== */
/* =========================== */
struct guest_info* guest_list = NULL; // lista de guests conectados
int num_guests = 0; // número de guests conectados


/* ============== */
/* ==== MAIN ==== */
/* ============== */
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Uso: %s [manager|guest]\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "manager") == 0) {
        // "manager" cria threads para os serviços de descoberta e monitoramento
        pthread_t discovery_thread, monitoring_thread;

        if (pthread_create(&discovery_thread, NULL, discovery_service, NULL) != 0) {
            perror("[Discovery|Manager] Erro ao criar thread");
            exit(1);
        }

        if (pthread_create(&monitoring_thread, NULL, monitoring_service, NULL) != 0) {
            perror("[Monitoring|Manager] Erro ao criar thread");
            exit(1);
        }

        pthread_join(discovery_thread, NULL);
        pthread_join(monitoring_thread, NULL);
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

        // coleta mac address e nome da interface
        char *mac = get_mac_address();
        char *interface = get_network_interface();

        // coloca mac e interface em uma string para enviar ao "manager"
        char *message = (char *) malloc(strlen(mac) + strlen(interface) + 2);
        sprintf(message, "%s,%s", mac, interface);

        if (sendto(discovery_socket, message, strlen(message), 0, (struct sockaddr*)&discovery_addr, sizeof(discovery_addr)) < 0) {
            perror("[Discovery|Guest] Erro ao enviar mensagem ao manager (JOIN_REQUEST)");
            exit(1);
        }

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
        monitoring_addr.sin_port = htons(MONITORING_PORT);
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
                perror("[Monitoring|Guest] Erro ao receber mensagem do manager (SLEEP_STATUS_REQUEST)");
                continue;
            }

            buffer[recv_len] = '\0';
            printf("[Monitoring|Guest] Mensagem de manager (%s:%d): %s\n", inet_ntoa(manager_addr.sin_addr), ntohs(manager_addr.sin_port), buffer);

            // verifica se é mensagem do tipo SLEEP_STATUS_REQUEST
            if (strcmp(buffer, "SLEEP_STATUS_REQUEST") == 0) {
                // envia SLEEP_STATUS_RESPONSE
                const char* response = "awaken";

                ssize_t send_len = sendto(monitoring_socket, response, strlen(response), 0, (struct sockaddr*)&manager_addr, manager_addr_len);
                if (send_len < 0) {
                    perror("[Monitoring|Guest] Erro ao enviar mensagem ao manager (SLEEP_STATUS_RESPONSE)");
                    continue;
                }
            }
        }

        close(monitoring_socket);
    }

    return 0;
}


/* =================================== */
/* ==== IMPLEMENTAÇÃO DAS FUNÇÕES ==== */
/* =================================== */
void* discovery_service(void* arg) {
    // cria e configura socket para se comunicar com "guest"
    int discovery_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("[Discovery|Manager] Erro ao criar o socket");
        exit(1);
    }

    // modo broadcast
    int broadcast_enable = 1;
    if (setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("[Discovery|Manager] Erro ao habilitar o modo Broadcast");
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
    while (1) {
        // recebe mensagem do guest com "mac,interface"
        ssize_t recv_len = recvfrom(discovery_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&guest_addr, &guest_addr_len);
        if (recv_len < 0) {
            perror("[Discovery|Manager] Erro ao receber mensagem do guest (JOIN_REQUEST)");
            continue;
        }

        // separa as strings em mac e interface
        char *token = strtok(buffer, ",");
        char *mac = token;

        token = strtok(NULL, ",");
        char *interface = token;
        strtok(interface, "\n");

        // chama serviço de gerenciamento para atualizar a lista de guests
        management_service(inet_ntoa(guest_addr.sin_addr), ntohs(guest_addr.sin_port), mac, interface);
    }

    close(discovery_socket);

    return NULL;
}

void* monitoring_service(void* arg) {
    // cria e configura socket para se comunicar com o "guest"
    int monitoring_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitoring_socket < 0) {
        perror("[Monitoring|Manager] Erro ao criar o socket");
        exit(1);
    }

    int enable = 1;
    if (setsockopt(monitoring_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("[Monitoring|Manager] Erro ao habilitar o modo Padrão");
        exit(1);
    }

    struct sockaddr_in guest_addr;
    socklen_t guest_addr_len = sizeof(guest_addr);
    guest_addr.sin_family = AF_INET;
    guest_addr.sin_port = htons(MONITORING_PORT);

    while (1) {
        // itera a lista de guests
        for (int i = 0; i < num_guests; i++) {
            struct guest_info guest = guest_list[i];
            guest_addr.sin_addr.s_addr = inet_addr(guest.ip);

            // envia uma mensagem do tipo SLEEP_STATUS_REQUEST para o guest iterado
            const char* message = "SLEEP_STATUS_REQUEST";
            ssize_t send_len = sendto(monitoring_socket, message, strlen(message), 0, (struct sockaddr*)&guest_addr, sizeof(guest_addr));
            if (send_len < 0) {
                perror("[Monitoring|Manager] Erro ao enviar a mensagem ao guest (SLEEP_STATUS_REQUEST)");
                continue;
            }

            // aguarda uma mensagem do tipo SLEE_STATUS_RESPONSE do guest iterado
            char buffer[MAX_MSG_LEN];
            ssize_t recv_len = recvfrom(monitoring_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&guest_addr, &guest_addr_len);
            if (recv_len < 0) {
                perror("[Monitoring|Manager] Erro ao receber a mensagem do guest (SLEEP_STATUS_RESPONSE)");
                continue;
            }

            buffer[recv_len] = '\0';
            printf("[Monitoring|Manager] Status do guest (%s:%d): %s\n", inet_ntoa(guest_addr.sin_addr), ntohs(guest_addr.sin_port), buffer);

            // atualiza o guest com o estado atual de sono (awaken/asleep)
            guest.status = buffer;
            guest_list[i] = guest;
        }

        sleep(5);

        show_guest_list();
    }

    close(monitoring_socket);

    return NULL;
}

void management_service(char* ip, int port, char* mac, char* interface) {
    // verifica se o guest já está na lista
    // a estrutura de lista escolhida é uma LSE (com ponteiro apenas para o próximo guest)
    struct guest_info* curr = guest_list;
    while (curr != NULL) {
        if (strcmp(curr->ip, ip) == 0 && curr->port == port) {
            printf("[Management|Manager] Guest (%s:%d) ja se encontra na lista\n", ip, port);
            return;
        }
        curr = curr->next;
    }

    // cria novo guest
    struct guest_info* new_guest = (struct guest_info*)malloc(sizeof(struct guest_info));
    if (new_guest == NULL) {
        perror("[Management|Manager] Erro ao alocar memoria para o guest");
        exit(1);
    }

    // preenche a estrutura
    // obs.: ainda não se conhece o estado do guest
    strcpy(new_guest->ip, ip);
    new_guest->port = port;
    strcpy(new_guest->mac_address, mac);
    strcpy(new_guest->network_interface, interface);
    new_guest->next = NULL;

    // adiciona o novo guest na lista
    if (guest_list == NULL) {
        guest_list = new_guest;
    }
    else {
        curr = guest_list;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_guest;
    }

    num_guests++;

    printf("[Management|Manager] Novo guest \"%d\" incluido.\n", num_guests-1);
    printf("[Management|Manager] IP:PORTA - (%s:%d).\n", ip, port); 
    printf("[Management|Manager] MAC/INTERFACE - (%s/%s).\n", mac, interface);
}

void interface_service() {
    // aguarda o usuário entrar com o comando desejado
    // pode ser:
    //   list   - lista os guests atualmente conectados
    //   wakeup - acorda um guest por seu hostname (id)
    //   exit   - guest encerra participação no serviço de gerenciamento de sono
    while (1) {
        char input[10];
        printf("[Interface] Entre com um comando (list/wakeup/exit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            continue;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "list") == 0) {
            printf("[Interface] Comando LIST executado.\n");
            show_guest_list();
        }
        else if (strcmp(input, "wakeup") == 0) {
            printf("[Interface] Comando WAKEUP executado.\n");
            // send_wol_packet();
        }
        else if (strcmp(input, "exit") == 0) {
            printf("[Interface] Comando EXIT executado.\n");
            exit(0);
        }
        else {
            printf("[Interface] Comando invalido.\n");
        }
    }
}

void show_guest_list() {
    // imprime a lista de guests
    printf("\n[Interface] Lista de Guests\n");
    printf("%-4s%-15s%-10s%-10s\n", "ID", "IP", "Port", "Status");
    for (int i = 0; i < num_guests; i++) {
        printf("%-4d%-15s%-10d%-10s\n", i, guest_list[i].ip, guest_list[i].port, guest_list[i].status);
    }
    printf("\n");
}

char* get_mac_address() {
    // obtém o endereço mac do guest usando chamadas de sistema e regex
    char mac_addr[18];

    FILE *fp = popen("ifconfig | \
                      grep 'ether ' | \
                      awk '{print $2}'", "r");
    if (fp == NULL) {
        perror("[Discovery|Guest] Erro ao obter o endereço MAC");
        exit(1);
    }

    fgets(mac_addr, sizeof(mac_addr), fp);
    pclose(fp);

    char* mac = (char*) malloc(strlen(mac_addr) + 1);
    strcpy(mac, mac_addr);

    return mac;
}

char* get_network_interface() {
    // obtém o nome da interface de rede do guest usando chamadas de sistema e regex
    char interface_name[10];

    FILE *fp = popen("ifconfig | \
                      grep -B 1 $(ifconfig | \
                      grep -m 1 -o '^[^ ]*') | \
                      awk '/^[^ ]/{print $1}' | \
                      sed 's/.$//'", "r");
    if (fp == NULL) {
        perror("[Discovery|Guest] Erro ao obter o nome da interface da rede");
        exit(1);
    }

    fgets(interface_name, sizeof(interface_name), fp);
    pclose(fp);

    char* interface = (char*) malloc(strlen(interface_name) + 1);
    strcpy(interface, interface_name);

    return interface;
}
