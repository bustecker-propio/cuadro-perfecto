#include <Arduino.h>
#include "credentials.h"

// Credenciales de Red WiFi (opcionales, ya que WiFiManager guarda las redes anteriores)
const char* ssid = "TU_WIFI_SSID";
const char* password = "TU_WIFI_PASSWORD";

// Credenciales reales de tu broker HiveMQ Cloud (Seguro con TLS)
const char* mqtt_server = "9586e4c7daae4cc7bfca094ad69aac50.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "django_server";
const char* mqtt_pass = "1974.Ale";

// Tópicos MQTT dinámicos que se sobrescribirán en tiempo de ejecución
String mqtt_topic_galeria = "";
String mqtt_topic_dispositivo = "";

// Token de acceso cargado dinámicamente desde /token.txt (emparejamiento inteligente)
char device_token[65] = "";

// URL de la API local o pública para sincronizar el catálogo y vincular el cuadro
const char* backend_sync_url = "http://192.168.1.106:8000/api/galeria/marco/sincronizar/";
const char* backend_vincular_codigo_url = "http://192.168.1.106:8000/api/galeria/marco/vincular/codigo/";
const char* backend_vincular_estado_url = "http://192.168.1.106:8000/api/galeria/marco/vincular/estado/";
