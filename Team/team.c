#include "team.h"

TEAMConfig config;
t_log* logger;
t_log* logger_server;
pthread_t appeared_thread;
pthread_t localized_thread;
pthread_t caught_thread;
t_config *config_file;
// Estructura clave-valor para manejar los objetivos globales, la clave es el nombre y el valor es la cantidad necesitada
t_dictionary* objetivo_global;

int main() {
    MessageType test = ABC;
    pthread_t server_thread;

    // Leo archivo de configuracion, si no lo encuentro salgo del proceso
    if (read_config_options() == -1) {
        printf("No se encontro archivo de configuracion, saliendo.");
        return -1;
    }

    // Inicializo el log, si no pude salgo del proceso
    if (start_log() == -1) {
        printf("No se pudo inicializar el log en la ruta especificada, saliendo.");
        return -1;
    }

    //Creo el servidor para que el GameBoy me mande mensajes
    pthread_create(&server_thread, NULL, server_function, NULL);
    
    //Creo 3 hilos para suscribirme a las colas globales
    subscribe_to_queues();

    initialize_structures();

    //Esta linea esta solo de prueba
    send_to_server(test);

    //Joineo el hilo main con el del servidor para el GameBoy, en realidad ninguno de los 2 tendria que terminar nunca
    pthread_join(server_thread, NULL);

    config_destroy(config_file);
    log_destroy(logger);
}

int read_config_options() {

    config_file = config_create("../team.config");
    if (!config_file) {
        return -1;
    }
    config.posiciones_entrenadores = config_get_array_value(config_file, "POSICIONES_ENTRENADORES");
    config.pokemon_entrenadores = config_get_array_value(config_file, "POKEMON_ENTRENADORES");
    config.objetivos_entrenadores = config_get_array_value(config_file, "OBJETIVOS_ENTRENADORES");
    config.tiempo_reconexion = config_get_int_value(config_file, "TIEMPO_RECONEXION");
    config.retardo_ciclo_cpu = config_get_int_value(config_file, "RETARDO_CICLO_CPU");
    config.algoritmo_planificacion = config_get_string_value(config_file, "ALGORITMO_PLANIFICACION");
    config.quantum = config_get_int_value(config_file, "QUANTUM");
    config.estimacion_inicial = config_get_int_value(config_file, "ESTIMACION_INICIAL");
    config.ip_broker = config_get_string_value(config_file, "IP_BROKER");
    config.puerto_broker = config_get_int_value(config_file, "PUERTO_BROKER");
    config.ip_team = config_get_string_value(config_file, "IP_TEAM");
    config.puerto_team = config_get_int_value(config_file, "PUERTO_TEAM");
    config.log_file = config_get_string_value(config_file, "LOG_FILE");
    config.team_id = config_get_int_value(config_file, "TEAM_ID");
    return 1;
}

//TODO: cambiar el 1 por un 0 para la entrega
int start_log() {

    logger = log_create(config.log_file, "team", 1, LOG_LEVEL_TRACE);
    if (!logger) {
        return -1;
    }
    return 1;
}

void subscribe_to_queues() {

    // Levanto 3 hilos y en cada uno realizo una conexion al broker para cada una de las colas
    MessageType* appeared = malloc(sizeof(MessageType));
    *appeared = SUB_APPEARED;
    pthread_create(&appeared_thread, NULL, &subscribe_to_queue_thread, (void*)appeared);
    pthread_detach(appeared_thread);
    free(appeared);

    MessageType* localized = malloc(sizeof(MessageType));
    *localized = SUB_LOCALIZED;
    pthread_create(&localized_thread, NULL, &subscribe_to_queue_thread, (void*)localized);
    pthread_detach(localized_thread);
    free(localized);

    MessageType* caught = malloc(sizeof(MessageType));
    *caught = SUB_CAUGHT;
    pthread_create(&caught_thread, NULL, &subscribe_to_queue_thread, (void*)caught);
    pthread_detach(caught_thread);
    free(caught);
}

int connect_to_broker(){

    int client_socket;
    if((client_socket = create_socket()) == -1) {
        log_error(logger, "Error al crear el socket de cliente");
        return -1;
    }
    if(connect_socket(client_socket, config.ip_broker, config.puerto_broker) == -1){
        log_error(logger, "Error al conectarse al Broker");
        return -1;
    }
    return client_socket;
}

void disconnect_from_broker(int broker_socket) {
    close_socket(broker_socket);
}

void* subscribe_to_queue_thread(void* arg) {
    MessageType cola = *(MessageType*)arg;
    free(arg);

    // Me intento conectar y suscribir, la funcion no retorna hasta que no lo logre
    int broker = connect_and_subscribe(cola);

    //TODO: PROBAR ESTO
    // Me quedo en un loop infinito esperando a recibir cosas
    while (true) {

        MessageHeader* buffer_header = malloc(sizeof(MessageHeader));
        if(receive_header(broker, buffer_header) > 0) {

            // Recibo la confirmacion
            t_list* rta_list = receive_package(broker, buffer_header);
            int rta = *(int*) list_get(rta_list, 0);

            // Switch case que seleccione que hacer con la respuesta segun el tipo de cola
            // TODO: confirmar la recepcion con un send que mande un 1 o un ACK o algo de eso

            // Limpieza
            free(buffer_header);
            //TODO: eliminar la lista

        // Si surgio algun error durante el receive header, me reconecto y vuelvo a iterar
        } else {
            broker = connect_and_subscribe(cola);
        }
    }

    return null;
}

int connect_and_subscribe(MessageType cola) {
    int broker;
    bool connected = false;
    //Mientras que no este conectado itero
    while (!connected) {

        // Me conecto al Broker
        broker = connect_to_broker();

        // Si me pude conectar al Broker
        if (broker != -1) {

            // Me intento suscribir a la cola pasada por parametro
            connected = subscribe_to_queue(broker, cola);

            // Si no me pude conectar al Broker, me duermo y vuelvo a intentar en unos segundos
        } else {
            sleep(config.tiempo_reconexion);
        }
    }

    return broker;
}

bool subscribe_to_queue(int broker, MessageType cola) {

    // Creo un paquete para la suscripcion a una cola
    t_paquete* paquete = create_package(cola);
    int* id = malloc(sizeof(int));
    *id = config.team_id;
    // TODO:Chequear que este bien lo del tamaño del id
    add_to_package(paquete, (void*) id, sizeof(int));

    // Envio el paquete, si no se puede enviar retorno false
    if(send_package(paquete, broker)  == -1){
        return false;
    }

    // Limpieza
    free(id);
    free_package(paquete);

    // Trato de recibir el encabezado de la respuesta
    MessageHeader* buffer_header = malloc(sizeof(MessageHeader));
    if(receive_header(broker, buffer_header) <= 0) {
        return false;
    }

    // Recibo la confirmacion
    t_list* rta_list = receive_package(broker, buffer_header);
    int rta = *(int*) list_get(rta_list, 0);

    // Limpieza
    free(buffer_header);
    //TODO: destruir la lista

    return rta == 1;
}

void* server_function(void* arg) {

    start_log_server();

    int server_socket;

    // La creacion de nuestro socket servidor puede fallar, si falla duermo y vuelvo a intentar en n segundos
    while ((server_socket = initialize_server()) == -1) {

        sleep(config.tiempo_reconexion);
    }

    start_server(server_socket, &new, &lost, &incoming);

    return null;
}

//TODO: terminar de implementar
void initialize_structures(){
    //Itero la lista de entrenadores, y creo un hilo por cada uno

    char** ptr = config.posiciones_entrenadores;
    int pos = 0;
    objetivo_global = dictionary_create();
    t_list* entrenadores = list_create();
    //La lista de hilos deberia ser una variable global?
    t_list* hilos = list_create();

    //Itero el array de posiciones de entrenadores
    for (char* coordenada = *ptr; coordenada; coordenada=*++ptr) {

        Entrenador* entrenador = (Entrenador*) malloc(sizeof(Entrenador));
        entrenador->objetivos_particular = dictionary_create();
        entrenador->stock_pokemons = dictionary_create();
        //Inicializo el nuevo hilo
        //pthread_t trainer_thread = malloc(sizeof(pthread_t));

        // Definir si el array de entrenadores tendria que ser global o si no importa
        log_info(logger, "entrenador: %d", pos);

        // Obtengo los objetivos y los pokemones que posee el entrenador actual
        char** objetivos_entrenador = string_split(config.objetivos_entrenadores[pos], "|");
        char** pokemon_entrenador = string_split(config.pokemon_entrenadores[pos], "|");
        char** posiciones = string_split(coordenada, "|");
        add_global_objectives(objetivos_entrenador, pokemon_entrenador);

        //Instancio la estructura entrenador con los datos recogidos del archivo de configuracion

        add_to_dictionary(objetivos_entrenador, entrenador->objetivos_particular);
        add_to_dictionary(pokemon_entrenador, entrenador->stock_pokemons);
        sscanf(posiciones[0], "%d", &entrenador->pos_x);
        sscanf(posiciones[1], "%d", &entrenador->pos_y);
        list_add(entrenadores, (void*) entrenador);
        pos++;

        //TODO: Usar un array de hilos, pendiente ver con eze
//        pthread_create(&trainer_thread, NULL, scheduling, NULL);
        // Agregar hilo a la lista lista
//        list_add(hilos, (void*) trainer_thread);
    }
    // Iterar lista de hilos y joinear, esto habria que hacerlo en main?

}

void add_to_dictionary(char** cosas_agregar, t_dictionary* diccionario){

    // Itero la lista de pokemones objetivo de un entrenador dado
    for (char* pokemon = *cosas_agregar; pokemon ; pokemon = *++cosas_agregar) {

        // Verifico si ya existia la necesidad de este pokemon, si existe le sumo uno
        if (dictionary_has_key(diccionario, pokemon)) {

            //Por alguna razon que no pude descifrar, no funcionaba el put
            *(int*)dictionary_get(diccionario, pokemon) += 1;

            // Si no existia la necesidad la creo
        } else {

            int* necesidad = (int*)malloc(sizeof(int));
            *necesidad = 1;
            dictionary_put(diccionario, pokemon, (void*) necesidad);
        }
    }
}

void add_global_objectives(char** objetivos_entrenador, char** pokemon_entrenador) {

    int necesidad_actual;
    // Itero la lista de pokemones objetivo de un entrenador dado
    for (char* pokemon = *objetivos_entrenador; pokemon ; pokemon = *++objetivos_entrenador) {

        // Verifico si ya existia la necesidad de este pokemon, si existe le sumo uno
        if (dictionary_has_key(objetivo_global, pokemon)) {

            //Por alguna razon que no pude descifrar, no funcionaba el put
            *(int*)dictionary_get(objetivo_global, pokemon) += 1;

            // Si no existia la necesidad la creo
        } else {

            int* necesidad = (int*)malloc(sizeof(int));
            *necesidad = 1;
            dictionary_put(objetivo_global, pokemon, (void*) necesidad);
        }
    }

    // Itero la lista de pokemones que posee un entrenador dado, para restarle al objetivo global
    for (char* pokemon = *pokemon_entrenador; pokemon ; pokemon = *++pokemon_entrenador) {

        // Verifico si ya existia la necesidad de este pokemon, si existe le resto uno
        if (dictionary_has_key(objetivo_global, pokemon)) {

            *(int*)dictionary_get(objetivo_global, pokemon) -= 1;

            //TODO: verificar que no sean tan forros de poner un pokemon que nadie va a utilizar
            // Si no existia la necesidad la creo(con valor de -1)
        } else {

            int* necesidad = (int*)malloc(sizeof(int));
            *necesidad = 1;
            dictionary_put(objetivo_global, pokemon, (void*) necesidad);
        }
    }
}

// Donde se usaria esto?
void* scheduling(void* arg){
    return null;
}

void start_log_server() {

    //Cambiar 1 por 0?
    logger_server=log_create("../servidor.log", "servidor", 1, LOG_LEVEL_TRACE);
}

int initialize_server(){

    int server_socket;
    int port = config.puerto_team;

    if((server_socket = create_socket()) == -1) {
        log_error(logger_server, "Error creating socket");
        return -1;
    }
    if((bind_socket(server_socket, port)) == -1) {
        log_error(logger_server, "Error binding socket");
        return -1;
    }

    return server_socket;
}

void new(int server_socket, char * ip, int port){
    log_info(logger_server,"Nueva conexion: Socket %d, Puerto: %d", server_socket, port);
}

void lost(int server_socket, char * ip, int port){
    log_info(logger_server, "Conexion perdida");
}

void incoming(int server_socket, char* ip, int port, MessageHeader * headerStruct){

    t_list* paquete_recibido = receive_package(server_socket, headerStruct);

    void* mensaje = list_get(paquete_recibido,0);

    switch(headerStruct -> type){

        case SUB_APPEARED:
            printf("APPEARED_POKEMON\n");
            t_paquete* paquete = create_package(SUB_APPEARED);
            int* rta = malloc(sizeof(int));
            *rta = 1;
            add_to_package(paquete, (void*) rta, sizeof(int));

            // Envio el paquete, si no se puede enviar retorno false
            send_package(paquete, server_socket);
            break;
        case SUB_LOCALIZED:
            printf("LOCALIZED_POKEMON\n");
            t_paquete* paquete2 = create_package(SUB_APPEARED);
            int* rta2 = malloc(sizeof(int));
            *rta2 = 1;
            add_to_package(paquete2, (void*) rta2, sizeof(int));

            // Envio el paquete, si no se puede enviar retorno false
            send_package(paquete2, server_socket);
            break;
        case SUB_CAUGHT:
            printf("SUB_CAUGHT\n");
            break;
        case APPEARED_POK:
            printf("APPEARED_POKEMON\n");
            break;
        case LOCALIZED_POK:
            printf("LOCALIZED_POKEMON\n");
            break;
        case CAUGHT_POK:
            printf("CAUGHT_POKEMON\n");
            break;
        default:
            printf("la estas cagando compa\n");
            break;
    }

}

//----------------------------------------HELPERS----------------------------------------

//Funcion de prueba
void send_to_server(MessageType mensaje){
    int broker = connect_to_broker();
    t_paquete* paquete = create_package(mensaje);


    char* enviar = malloc(50);
    strcpy(enviar, "test");

    add_to_package(paquete,(void*) enviar, strlen("test")+1);


    if(send_package(paquete, broker)  == -1){
        printf("No se pudo mandar");
    }
    disconnect_from_broker(broker);
}
