#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include <Arduino.h>

// Credenciales de Red WiFi (si se usan de respaldo, ya que WiFiManager las autogestiona)
extern const char* ssid;
extern const char* password;

// Credenciales del Broker MQTT de HiveMQ Cloud (Seguro)
extern const char* mqtt_server;
extern const int mqtt_port;
extern const char* mqtt_user;
extern const char* mqtt_pass;

// Tópicos MQTT dinámicos (se rellenan al consultar el catálogo)
extern String mqtt_topic_galeria;
extern String mqtt_topic_dispositivo;

// Token de acceso cargado dinámicamente desde /token.txt
extern char device_token[65];

// URLs de la API de Django
extern const char* backend_sync_url;
extern const char* backend_vincular_codigo_url;
extern const char* backend_vincular_estado_url;

#endif // CREDENTIALS_H
