#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#include <pthread.h>

#define PORT 8080
#define BUF_SIZE 1024
#define username_max_length 50
#define password_max_length 20
#define email_max_length 50
#define read_buffer_length 512
#define write_buffer_length 1024
#define database_host "localhost"
#define database_username "root"
#define database_password "mysql"
#define database_name "bd"

// Estructura para almacenar información del cliente
typedef struct {
    int sock_conn;
} client_info;

// Función para conectarse a la base de datos MySQL
MYSQL* connect_to_db() {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(EXIT_FAILURE);
    }

    // Conectar a la base de datos
    if (mysql_real_connect(conn, database_host, database_username, database_password, database_name, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }
    return conn;
}

// Funciones auxiliares
int dime_si_usuario_y_contra_son_correctas(const char *nombre_usuario, const char *contrasena, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT * FROM jugador WHERE username='%s' AND Contrasenya='%s'", nombre_usuario, contrasena);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);
    return num_rows > 0 ? 1 : 0; // 1 si usuario y contraseña son correctas, 0 si no
}

int anadir_usario_a_la_base_de_datos(const char *username, const char *contrasena, const char *correo, const char *fecha, MYSQL *conn) {
    char query[256];
    sprintf(query, "INSERT INTO jugador (Username, Contrasenya, Correo, FechaDeNacimiento) VALUES ('%s', '%s', '%s', '%s')", username, contrasena, correo, fecha);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed. Error: %s\n", mysql_error(conn));
        return 0; // Error al insertar
    }
    return 1; // Inserción exitosa
}

int dime_si_usuario_existe(const char *username, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT * FROM jugador WHERE Username='%s'", username);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);
    return num_rows > 0 ? 1 : 0; // 1 si el usuario existe, 0 si no
}

int dime_si_correo_existe(const char *correo, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT * FROM jugador WHERE Correo='%s'", correo);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);
    return num_rows > 0 ? 1 : 0; // 1 si el correo existe, 0 si no
}

int numero_de_partidas_jugadas_en_X_intervalo_de_tiempo(const char *dia1, const char *dia2, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT COUNT(*) FROM partidas WHERE FechayHoraFinal BETWEEN '%s' AND '%s'", dia1, dia2);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = atoi(row[0]);
    mysql_free_result(res);
    return count;
}

int devuelvaPartidasGanadas(const char *nombre_usuario, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT COUNT(*) FROM partidas WHERE ganador IN (SELECT ID FROM jugador WHERE Username='%s')", nombre_usuario);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = atoi(row[0]);
    mysql_free_result(res);
    return count;
}

int devuelvaPartidasPerdidas(const char *nombre_usuario, MYSQL *conn) {
    char query[256];
    sprintf(query, "SELECT COUNT(*) FROM partidas WHERE ganador NOT IN (SELECT ID FROM jugador WHERE Username='%s')", nombre_usuario);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = atoi(row[0]);
    mysql_free_result(res);
    return count;
}

void dame_todos_los_usuarios(char *todos, MYSQL *conn) {
    sprintf(todos, "");
    if (mysql_query(conn, "SELECT Username FROM jugador")) {
        fprintf(stderr, "SELECT failed. Error: %s\n", mysql_error(conn));
        return;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        strcat(todos, row[0]);
        strcat(todos, ", ");
    }
    mysql_free_result(res);
    // Remover la última coma y espacio
    if (strlen(todos) > 0) {
        todos[strlen(todos) - 2] = '\0';
    }
}

// Función para manejar la comunicación con el cliente
void* atenderClientes(void *arg) {
    client_info *client = (client_info *)arg;
    int sock_conn = client->sock_conn;
    char respuesta[write_buffer_length];
    char peticion[read_buffer_length];
    int stop = 0;

    MYSQL *conn = connect_to_db();

    while (!stop) {
        int ret = read(sock_conn, peticion, sizeof(peticion) - 1);
        if (ret <= 0) {
            stop = 1; // Termina si no hay datos o se cierra la conexión
            break;
        }
        peticion[ret] = '\0'; // Null-terminate the string
        printf("Petición: %s\n", peticion);

        char *p = strtok(peticion, "/");
        int codigo = atoi(p);
        char nombre_usuario[username_max_length];
        char contrasena[password_max_length];
        char correo[email_max_length];
        char fecha[10];
        char dia1[10];
        char dia2[10];

        if (codigo == 0) { // Desconectar
            stop = 1;
            strcpy(respuesta, "0/Desconexión realizada con éxito");
        } 
        else if (codigo == 1) { // Iniciar sesión
            p = strtok(NULL, "/");
            strcpy(nombre_usuario, p);
            p = strtok(NULL, "/");
            strcpy(contrasena, p);
            printf("Código: %d, Nombre: %s Contra: %s\n", codigo, nombre_usuario, contrasena);
            int valor = dime_si_usuario_y_contra_son_correctas(nombre_usuario, contrasena, conn);
            strcpy(respuesta, valor == 1 ? "Login" : "Permiso denegado");
        } 
        else if (codigo == 2) { // Contar partidas en un rango de fechas
            p = strtok(NULL, "/");
            strcpy(dia1, p);
            p = strtok(NULL, "/");
            strcpy(dia2, p);
            printf("Código: %d, Dia 1: %s Dia 2: %s\n", codigo, dia1, dia2);
            int value = numero_de_partidas_jugadas_en_X_intervalo_de_tiempo(dia1, dia2, conn);
            sprintf(respuesta, "2/%d", value);
        } 
        else if (codigo == 3) { // Registrar usuario
            p = strtok(NULL, "/");
            strcpy(nombre_usuario, p);
            p = strtok(NULL, "/");
            strcpy(contrasena, p);
            p = strtok(NULL, "/");
            strcpy(correo, p);
            p = strtok(NULL, "/");
            strcpy(fecha, p);
            printf("Código: %d, Usuario: %s Contra: %s Correo: %s Fecha: %s\n", codigo, nombre_usuario, contrasena, correo, fecha);
            int registro = anadir_usario_a_la_base_de_datos(nombre_usuario, contrasena, correo, fecha, conn);
            if (registro == 1) {
                strcpy(respuesta, "Registro realizado correctamente.");
            } else {
                if (dime_si_usuario_existe(nombre_usuario, conn) == 1) {
                    strcpy(respuesta, "Usuario existente");
                } else if (dime_si_correo_existe(correo, conn) == 1) {
                    strcpy(respuesta, "Correo existente");
                }
            }
        } 
        else if (codigo == 4) { // Contar partidas ganadas
            p = strtok(NULL, "/");
            strcpy(nombre_usuario, p);
            int partidas_ganadas = devuelvaPartidasGanadas(nombre_usuario, conn);
            sprintf(respuesta, "4/%d", partidas_ganadas);
        } 
        else if (codigo == 5) { // Contar partidas perdidas
            p = strtok(NULL, "/");
            strcpy(nombre_usuario, p);
            sprintf(respuesta, "5/%d", devuelvaPartidasPerdidas(nombre_usuario, conn));
        } 
        else if (codigo == 6) { // Listar todos los usuarios
            char todos[90000];
            dame_todos_los_usuarios(todos, conn);
            sprintf(respuesta, "6/%s", todos);
        } 
        else { // Código no reconocido
            strcpy(respuesta, "Código no reconocido");
        }

        // Enviar la respuesta al cliente
        write(sock_conn, respuesta, strlen(respuesta));
        printf("Respuesta enviada: %s\n", respuesta);
    }

    close(sock_conn);
    mysql_close(conn);
    free(client);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Opción para reutilizar el puerto
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Asociar el socket con el puerto
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    while (1) {
        // Aceptar nuevas conexiones
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Nueva conexión aceptada.\n");

        // Crear una nueva estructura de cliente
        client_info *client = malloc(sizeof(client_info));
        client->sock_conn = new_socket;

        // Crear un nuevo hilo para manejar al cliente
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, atenderClientes, (void *)client);
        pthread_detach(thread_id); // Desacoplar el hilo
    }

    return 0;
}
//version ultima