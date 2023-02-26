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


/* ================================ */
/* ==== ESQUELETOS DAS FUNÇÕES ==== */
/* ================================ */
/* Nome: send_discovery_message().
   Descrição: envia mensagem ao manager indicando que deseja participar do serviço
              de gerenciamento de sono, em seguida aguardando uma resposta contendo
              o id do guest no serviço.
   Entrada: nenhuma.
   Saída: nenhuma. */
void  send_discovery_message();

/* Nome: join_monitoring_service().
   Descrição: executa um loop em que envia e recebe mensagens ao manager ao longo de todo
              o programa. as mensagens podem ser dos tipos: SLEEP STATUS REQUEST,
              SLEEP STATUS RESPONSE, SLEEP SERVICE QUIT, SLEEP QUIT ACKNOWLEDGE.
   Entrada: nenhuma.
   Saída: nenhuma. */
void  join_monitoring_service();

/* Nome: guest_interface_service().
   Descrição: executa o serviço de interface, que apresenta uma lista de comandos que o
              guest pode executar. o único comando visível para o guest é: exit (encerra
              sua participação no serviço).
   Entrada: nenhuma.
   Saída: nenhuma. */
void* guest_interface_service();

/* Nome: get_mac_address().
   Descrição: coleta o endereço mac do usuário.
   Entrada: nenhuma.
   Saída: string contendo o endereço mac. */
char* get_mac_address();

/* Nome: get_hostname().
   Descrição: coleta o hostname do usuário.
   Entrada: nenhuma.
   Saída: string contendo o hostname. */
char* get_hostname();