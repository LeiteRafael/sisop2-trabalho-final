/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "manager.h"


/* =========================== */
/* ==== VARIÁVEIS GLOBAIS ==== */
/* =========================== */
pthread_mutex_t guest_list_mutex = PTHREAD_MUTEX_INITIALIZER;
struct guest_info* guest_list = NULL; // lista de guests conectados
int num_guests = 0; // número de guests conectados


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
        // recebe mensagem do guest com "mac, hostname"
        ssize_t recv_len = recvfrom(discovery_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&guest_addr, &guest_addr_len);
        if (recv_len < 0) {
            perror("[Discovery|Manager] Erro ao receber mensagem do guest (JOIN_REQUEST)");
            continue;
        }

        // separa as strings em mac e hostname
        char *token = strtok(buffer, ",");
        char *mac = token;

        token = strtok(NULL, ",");
        char *hostname = token;
        strtok(hostname, "\n");

        // chama serviço de gerenciamento para atualizar a lista de guests
        management_service(inet_ntoa(guest_addr.sin_addr), ntohs(guest_addr.sin_port), mac, hostname, "unknown");
        guest_list[num_guests-1].id = num_guests-1;

        // envia uma mensagem para o guest contendo seu id (hostname) no serviço
        char message[2];
        snprintf(message, sizeof(message), "%d", num_guests-1);
        
        ssize_t send_len = sendto(discovery_socket, message, strlen(message), 0, (struct sockaddr*)&guest_addr, sizeof(guest_addr));
        if (send_len < 0) {
            perror("[Monitoring|Manager] Erro ao enviar a mensagem ao guest (SLEEP_STATUS_REQUEST)");
            continue;
        }
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

    struct timeval timeout;
    timeout.tv_sec = 6;
    timeout.tv_usec = 0;
    if (setsockopt(monitoring_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("[Monitoring|Guest] Erro ao habilitar o modo Timeout");
        exit(1);
    }

    struct sockaddr_in guest_addr;
    socklen_t guest_addr_len = sizeof(guest_addr);
    guest_addr.sin_family = AF_INET;

    while (1) {
        int i = 0;
        struct guest_info* curr_guest = guest_list;
        while (curr_guest != NULL) {
            // cria a porta com o id do guest
            char guest_port[10];
            snprintf(guest_port, sizeof(guest_port), "%d", MONITORING_PORT + i);

            // atualiza a porta e o ip do socket com a porta do guest iterado
    		inet_aton(curr_guest->ip, &guest_addr.sin_addr);
            guest_addr.sin_port = htons(atoi(guest_port));

            // envia uma mensagem do tipo SLEEP_STATUS_REQUEST para o guest iterado
            const char* message = "SLEEP_STATUS_REQUEST";
            ssize_t send_len = sendto(monitoring_socket, message, strlen(message), 0, (struct sockaddr*)&guest_addr, sizeof(guest_addr));
            if (send_len < 0) {
                perror("[Monitoring|Manager] Erro ao enviar a mensagem ao guest (SLEEP_STATUS_REQUEST)");
                continue;
            }

            // aguarda uma mensagem do tipo SLEEP_STATUS_RESPONSE do guest iterado
            char buffer[MAX_MSG_LEN];
            ssize_t recv_len = recvfrom(monitoring_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&guest_addr, &guest_addr_len);
            if (recv_len < 0) {
                management_service(curr_guest->ip, curr_guest->port, curr_guest->mac_address, curr_guest->hostname, "asleep");
                continue;
            }
            
            if (strcmp(buffer, "awaken") == 0) {
                management_service(curr_guest->ip, curr_guest->port, curr_guest->mac_address, curr_guest->hostname, "awaken");
            }
            else if (strcmp(buffer, "SLEEP_SERVICE_QUIT") == 0) {
                // envia uma mensagem do tipo SLEEP_QUIT_ACKNOWLEDGE para o guest iterado
                const char* message = "SLEEP_QUIT_ACKNOWLEDGE";
                ssize_t send_len = sendto(monitoring_socket, message, strlen(message), 0, (struct sockaddr*)&guest_addr, sizeof(guest_addr));
                if (send_len < 0) {
                    perror("[Monitoring|Manager] Erro ao enviar a mensagem ao guest (SLEEP_QUIT_ACKNOWLEDGE)");
                    continue;
                }

                remove_guest(curr_guest->id);
                //update_all_guest_ids();
            }

            curr_guest = curr_guest->next; // move para o próximo guest na lista
            i++;
        }

        sleep(2);
    }

    close(monitoring_socket);

    return NULL;
}

void management_service(char* ip, int port, char* mac, char* hostname, char* buffer) {
    // protege o acesso simultâneo
    pthread_mutex_lock(&guest_list_mutex);

    // verifica se o guest já está na lista
    // a estrutura de lista escolhida é uma LSE (com ponteiro apenas para o próximo guest)
    struct guest_info* curr = guest_list;
    while (curr != NULL) {
        if (strcmp(curr->ip, ip) == 0 && curr->port == port) {
            // atualiza a estrutura do guest com o novo status
            strcpy(curr->status, buffer);

            // libera o mutex antes de retornar
            pthread_mutex_unlock(&guest_list_mutex);
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
    new_guest->id = num_guests;
    strcpy(new_guest->status, buffer);
    strcpy(new_guest->hostname, hostname);
    strcpy(new_guest->mac_address, mac);
    strcpy(new_guest->ip, ip);
    new_guest->port = port;
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

    // libera o mutex após a atualização
    pthread_mutex_unlock(&guest_list_mutex);
}

void* manager_interface_service() {
    // identificação se o usuário é "manager" ou "guest"
    char* user_type = "manager";

    printf("[Interface] Servico de Gerenciamento de Sono\n\n");
    printf("[Interface] Comandos disponiveis:\n");
    printf("[Interface] list                   - Lista os \"guests\" participantes do servico.\n");
    printf("[Interface] wakeup <hostname> <id> - Envia o pacote WOL para o \"guest\" de <hostname> e <id>.\n");
    printf("[Interface] cls                    - Limpa sua tela.\n");
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

        if (strcmp(command, "list") == 0) {
            show_guest_list();
        }
        else if (strcmp(command, "wakeup") == 0) {
            char* hostname;
            int id;

            hostname = strtok(NULL, " ");
            if (hostname == NULL) {
                printf("\n[Interface] Uso: wakeup <hostname> <id>.\n\n");
                continue;
            }

            id = atoi(strtok(NULL, " "));

            send_wol_packet(hostname, id);
        }
        else if (strcmp(command, "exit") == 0) {
            printf("\n[Interface] Encerrando sua participacao no servico.\n\n");

            // destrói o mutex e encerra o programa
            pthread_mutex_destroy(&guest_list_mutex);
            exit(0);
        }
        else if (strcmp(command, "cls") == 0) {
            system("clear");
            
            printf("[Interface] Servico de Gerenciamento de Sono\n\n");
            printf("[Interface] Comandos disponiveis:\n");
            printf("[Interface] list                   - Lista os \"guests\" participantes do servico.\n");
            printf("[Interface] wakeup <hostname> <id> - Envia o pacote WOL para o \"guest\" de <hostname> e <id>.\n");
            printf("[Interface] cls                    - Limpa sua tela.\n");
            printf("[Interface] exit                   - Encerra sua participacao no servico.\n");
        }
        else {
            printf("\n[Interface] Comando invalido.\n\n");
        }
    }
}

void remove_guest(int id) {
    // lock mutex antes de mexer na lista
    pthread_mutex_lock(&guest_list_mutex);

    // percorre a lista para encontrar o guest a ser removido
    struct guest_info* curr_guest = guest_list;
    struct guest_info* prev_guest = NULL;

    while (curr_guest != NULL && curr_guest->id != id) {
        prev_guest = curr_guest;
        curr_guest = curr_guest->next;
    }

    if (curr_guest == NULL) {
        printf("[Management|Manager] Guest com ID %d nao encontrado na lista\n", id);
        pthread_mutex_unlock(&guest_list_mutex);
        return;
    }

    // atualiza os IDs dos guests após a remoção
    if (curr_guest->id == 0) {
        struct guest_info* temp_guest = curr_guest->next;
        while (temp_guest != NULL) {
            temp_guest->id = temp_guest->id - 1;
            temp_guest = temp_guest->next;
        }
    } else {
        struct guest_info* temp_guest = curr_guest->next;
        while (temp_guest != NULL) {
            temp_guest->id = temp_guest->id - 1;
            temp_guest = temp_guest->next;
        }
    }

    // se o guest a ser removido é o primeiro da lista
    if (prev_guest == NULL) {
        guest_list = curr_guest->next;
    } else {
        prev_guest->next = curr_guest->next;
    }

    // atualiza o número de guests
    num_guests--;

    // libera a memória alocada para o guest removido
    free(curr_guest);

    // unlock mutex antes de terminar a função
    pthread_mutex_unlock(&guest_list_mutex);
}

void update_all_guest_ids () {
    int guest_dec = 0;

    struct guest_info* curr_guest = guest_list;

    // cria e configura socket para se comunicar com o "guest"
    int monitoring_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitoring_socket < 0) {
        perror("[Management|Manager] Erro ao criar o socket");
        exit(1);
    }

    int enable = 1;
    if (setsockopt(monitoring_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("[Management|Manager] Erro ao habilitar o modo Padrão");
        exit(1);
    }

    struct sockaddr_in guest_addr;
    socklen_t guest_addr_len = sizeof(guest_addr);
    guest_addr.sin_family = AF_INET;

    int i = num_guests+1; // tem 2 mas acha q é 3

    while (i >= 0) {
        // cria a porta com o id do guest
        char guest_port[10];
        snprintf(guest_port, sizeof(guest_port), "%d", MONITORING_PORT + curr_guest->id ); // 9000 9001 9002

        // atualiza a porta do socket com a porta do guest iterado
        guest_addr.sin_port = htons(atoi(guest_port));

        char message[MAX_MSG_LEN];
        snprintf(message, sizeof(message), "ID_UPDATE %d", curr_guest->id + guest_dec); // 0 0 1
        ssize_t send_len = sendto(monitoring_socket, message, strlen(message), 0, (struct sockaddr*)&guest_addr, sizeof(guest_addr));
        if (send_len < 0) {
            printf("erro");
            guest_dec = -1;
        }

        curr_guest = curr_guest->next;
        i--; // 1 0 -1
    }

    free(curr_guest);
    close(monitoring_socket);
}

void send_wol_packet(char* hostname, int id) {
    // itera a lista de guest até encontrar guest com certo "hostname, id"
    struct guest_info* curr_guest = guest_list;
    while (curr_guest != NULL && strcmp(curr_guest->hostname, hostname) != 0 && curr_guest->id != id) {
        curr_guest = curr_guest->next;
    }

    if (curr_guest == NULL) {
        fprintf(stderr, "[Pacote WOL|Manager] Guest com ID %d não encontrado na lista\n", id);
        return;
    }

    // prepara o comando WOL
    char command[150];
    snprintf(command, sizeof(command), "wakeonlan %s", curr_guest->mac_address);

    // executa o comando
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        perror("[Pacote WOL|Manager] Erro ao executar comando wakeonlan");
        return;
    }

    // lê a saída do comando
    char output[100];
    while (fgets(output, sizeof(output), fp) != NULL) {
        printf("\n[Pacote WOL|Manager] %s\n", output);
    }

    pclose(fp);
}

void show_guest_list() {
    struct guest_info* curr = guest_list;
    int i = 0;

    if (curr != NULL) {
        printf("\n[Interface] Lista de guests:\n");
        printf("|===========================================================================================|\n");
        printf("| %-18s | %-4s | %-18s | %-25s | %-12s |\n", "Hostname", "Id", "Ip", "MAC Address", "Status");
        while (curr != NULL) {
            printf("| %-18s | %-4d | %-18s | %-25s | %-12s |\n", curr->hostname, curr->id, curr->ip, curr->mac_address, curr->status);
            curr = curr->next;
            i++;
        }
        printf("|===========================================================================================|\n\n");
    }
    else{
        printf("\n[Interface] Lista de guests: VAZIA.\n\n");
    }
}
