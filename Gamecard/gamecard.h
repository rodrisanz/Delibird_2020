//
// Created by utnso on 07/04/20.
//

#ifndef TEAM_GAMECARD_H
#define TEAM_GAMECARD_H

#include <commons/config.h>
#include <commons/log.h>
#include <commLib/connections.h>
#include <commLib/structures.h>
#include "structures.h"
#include "tallGrass.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#endif //TEAM_GAMECARD_H

int leer_opciones_configuracion();

void liberar_opciones_configuracion();



void* mensaje_new_pokemon(void* parametros);

void* mensaje_get_pokemon(void* parametros);

void* mensaje_catch_pokemon(void* parametros);

t_pos_pokemon* obtener_sig_coordenada(t_file* archivo);

t_pos_pokemon* buscar_coordenadas(uint32_t x, uint32_t  y, t_file* archivo);

t_list* obtener_todas_coordenadas(t_file* archivo);

//------- funciones de conexion--------//
void subscribe_to_queues();

void* subscribe_to_queue_thread(void* arg);

int connect_and_subscribe(MessageType cola);

int connect_to_broker();

void disconnect_from_broker(int broker_socket);

bool subscribe_to_queue(int broker, MessageType cola);

int initialize_server();

void* server_function_gamecard(void* arg);

int envio_mensaje(t_paquete *paquete, char *ip, uint32_t puerto);