/* ================ */
/* ==== HEADER ==== */
/* ================ */
#include "manager.h"
#include "guest.h"


/* ============== */
/* ==== MAIN ==== */
/* ============== */
int main(int argc, char** argv) {
    system("clear");
    
    if (argc != 2) {
        printf("Uso: %s [manager|guest]\n", argv[0]);
        exit(1);
    }
    else {
        if (strcmp(argv[1], "manager") == 0) {
            // "manager" cria threads para os serviços de descoberta, monitoramento e interface
            pthread_t discovery_thread, monitoring_thread, interface_thread;

            if (pthread_create(&discovery_thread, NULL, discovery_service, NULL) != 0) {
                perror("[Discovery|Manager] Erro ao criar thread");
                exit(1);
            }

            if (pthread_create(&monitoring_thread, NULL, monitoring_service, NULL) != 0) {
                perror("[Monitoring|Manager] Erro ao criar thread");
                exit(1);
            }

            if (pthread_create(&interface_thread, NULL, manager_interface_service, (void*) argv[1]) != 0) {
                perror("[Interface|Manager] Erro ao criar thread");
                exit(1);
            }

            pthread_join(discovery_thread, NULL);
            pthread_join(monitoring_thread, NULL);
            pthread_join(interface_thread, NULL);
        }
        else if (strcmp(argv[1], "guest") == 0) {
            send_discovery_message();

            pthread_t interface_thread;
            if (pthread_create(&interface_thread, NULL, guest_interface_service, (void *) argv[1]) != 0) {
                perror("[Interface|Guest] Erro ao criar thread");
                exit(1);
            }

            join_monitoring_service();

            pthread_join(interface_thread, NULL);
        }
        else{
            printf("Nao existe o tipo de usuario \"%s\"\n", argv[1]);
            printf("Uso: %s [manager|guest]\n", argv[0]);
            exit(1);
        }
    }

    return 0;
}