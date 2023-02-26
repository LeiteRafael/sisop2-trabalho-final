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


/* ==================== */
/* ==== ESTRUTURAS ==== */
/* ==================== */
struct guest_info {
    char hostname[20*MAX_GUESTS_ALLOWED];
    char ip[16*MAX_GUESTS_ALLOWED];
    char mac_address[18*MAX_GUESTS_ALLOWED];
    char status[10*MAX_GUESTS_ALLOWED];

    int id;
    int port;

    struct guest_info* next;
};


/* ================================ */
/* ==== ESQUELETOS DAS FUNÇÕES ==== */
/* ================================ */
/* Nome: discovery_service(void* arg).
   Descrição: executa o sub-serviço de descoberta de usuários do tipo "guest". nele, o
              manager aguarda uma mensagem enviada por broadcast pelo guest, e coleta
              algumas informações sobre ele para armazenar na lista de guests. após isso,
              o manager retorna ao guest o id dele no serviço.
   Entrada: nenhuma.
   Saída: nenhuma. */
void* discovery_service(void* arg);

/* Nome: monitoring_service(void* arg).
   Descrição: executa o sub-serviço de monitoração dos guests da lista. nele, o manager
              envia mensagens do tipo "SLEEP_STATUS_REQUEST" para o guest, que deve retornar
              uma "SLEEP_STATUS_RESPONSE" ("awaken"/"asleep"), que servirá para o manager
              atualizar o estado de sono do guest na lista. outras mensagens podem ser recebidas
              pelo manager, como "SLEEP_SERVICE_QUIT", em que o manager deve responder com
              "SLEEP_SERVICE_ACKNOWLEDGE" após remover o guest da lista.
   Entrada: nenhuma.
   Saída: nenhuma. */
void* monitoring_service(void* arg);

/* Nome: management_service(char* ip, int port, char* mac, char* hostname, char* buffer).
   Descrição: executa o sub-serviço de gerenciamento de guests. nele, o manager deve inserir ou
              atualizar os campos da estrutura guest_info* da lista guest_list, que contém os
              guests participantes do serviço. quando chamada para um guest que já se encontra
              na lista, deve atualizar o estado de sono do guest.
   Entrada: string contendo o ip do guest, inteiro contendo a porta do guest, string contendo o
            endereço mac do guest, string contendo o hostname do guest e string contendo o estado
            de sono do guest.
   Saída: nenhuma. */
void  management_service(char* ip, int port, char* mac, char* hostname, char* buffer);

/* Nome: manager_interface_service().
   Descrição: executa o serviço de interface, que apresenta uma lista de comandos que o
              manager pode executar. o comandos visíveis para o manager são: list (apresenta uma
              lista contendo informações sobre todos os guests participantes do serviço), wakeup
              (envia o pacote mágico WoL para o guest com <hostname> e <id>), cls (limpa a tela)
              e exit (encerra sua participação no serviço).
   Entrada: nenhuma.
   Saída: nenhuma. */
void* manager_interface_service();

/* Nome: remove_guest(int id).
   Descrição: remove o guest com <id> da lista de guests.
   Entrada: inteiro contendo o id do guest.
   Saída: nenhuma. */
void  remove_guest(int id);

/* Nome: update_all_guest_ids().
   Descrição: atualiza a lista de guests com o novo id após a remoção
              de um guest da lista.
   Entrada: nenhuma.
   Saída: nenhuma. */
void  update_all_guest_ids();

/* Nome: send_wol_packet(char* hostname, int id).
   Descrição: envia o pacote mágico WoL para o guest com <hostname> e <id>.
   Entrada: string contendo o hostname do guest e inteiro contendo o id do guest.
   Saída: nenhuma. */
void  send_wol_packet(char* hostname, int id);

/* Nome: show_guest_list().
   Descrição: apresenta a lista de guests contendo informações sobre os guests da lista
              como seu hostname, id, ip, endereço mac e estado de sono.
   Entrada: nenhuma.
   Saída: nenhuma. */
void  show_guest_list();