#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#define MQTT_MAX_PACKET_SIZE 1024
#include "credentials.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h> // Usamos la memoria Flash interna del ESP32 como disco local
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFiManager.h>
#include <algorithm>
#include <vector>

// Inicialización de la pantalla TFT
TFT_eSPI tft = TFT_eSPI();

#include <WiFiClientSecure.h>

// Clientes para Red y MQTT (Seguro con TLS)
WiFiClientSecure espClientSecure;
PubSubClient mqttClient(espClientSecure);

// Estados de Funcionamiento
enum ModoFuncionamiento {
  MODO_SINCRONIZADO, // Muestra la foto activa del servidor
  MODO_CARRUSEL,     // Bucle automático de fotos locales
  MODO_MANUAL        // Cambia de foto con toques en la pantalla (touch)
};
ModoFuncionamiento modoActual = MODO_SINCRONIZADO;

// Estructuras de almacenamiento local
std::vector<int>
    listaFotoIds; // Lista de IDs de fotos guardadas en la memoria interna
int indiceFotoLocal = -1; // Índice de la foto que se está mostrando localmente
int idFotoActual = -1; // ID de la foto que se muestra actualmente en pantalla

// Variables de control de flujo y temporizadores (Banderas no bloqueantes)
String nuevaUrlImagen = "";
int nuevoIdImagen = -1;
bool tengoNuevaImagen = false;
bool tengoFotoBorrada = false;
int idFotoABorrar = -1;
String active_galeria_id = "";

String getRutaFoto(int id) {
  if (active_galeria_id.length() > 0) {
    return "/fotos/gal_" + active_galeria_id + "/" + String(id) + ".jpg";
  }
  return "/fotos/" + String(id) + ".jpg";
}

unsigned long ultimoIntentoReconexion = 0;
unsigned long ultimoCambioCarrusel = 0;
unsigned long intervaloCarrusel =
    10000; // No const para poder cambiarlo en caliente
bool fsInicializada = false;
bool menuAbierto = false; // Bandera para el menú principal
bool menuConfiguracionesAbierto =
    false; // Nueva bandera para el submenú de ajustes
bool necesitaSincronizarCatalogo =
    false; // Bandera para sincronización automática al conectar

// Variables para el control de la Notificación en Esquina Superior Derecha
// (Glassmorphic)
bool mostrarNotificacionActiva = false;
String textoNotificacion1 = "";
String textoNotificacion2 = "";
unsigned long tiempoNotificacion = 0;
bool ocultarNotificacionEnSiguienteFoto = false;
String usuarioUltimaFoto = "Alguien";

// Variables para el control de Gestos (Swipe) y toques (Tap)
bool estabaTocado = false;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint16_t touchLastX = 0;
uint16_t touchLastY = 0;
unsigned long touchStartTime = 0;

// Declaraciones de funciones
void sincronizarUltimaFoto();
bool mostrarImagenLocalPorId(int id);
void refrescarPantallaActual();
void guardarIntervaloCarrusel();
void cargarIntervaloCarrusel();
void dibujarCorazon(int16_t x, int16_t y, int16_t r, uint16_t color);
void dibujarNotificacionEsquina();
void dibujarMenuConfiguraciones(bool redibujarTodo);
void procesarTouchConfiguraciones(uint16_t x, uint16_t y);
void procesarArrastreSlider(uint16_t x, uint16_t y);
void dibujarMenuModos();
void procesarTouchMenu(uint16_t x, uint16_t y);
void sincronizarCatalogoCompleto();

// =========================================================================
// Función callback obligatoria para el decodificador de imágenes JPEG (TJpg)
// Renderiza bloque a bloque el JPEG directamente en la pantalla
// =========================================================================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft.height())
    return false;

  // Si la notificación está activa, aplicar el efecto de vidrio esmerilado
  // (glassmorphism) sobre los píxeles del bloque que caigan dentro del área de
  // la notificación (X:240-470, Y:10-65)
  if (mostrarNotificacionActiva && !menuAbierto &&
      !menuConfiguracionesAbierto) {
    int16_t notif_x_min = 240;
    int16_t notif_x_max = 470;
    int16_t notif_y_min = 10;
    int16_t notif_y_max = 65;

    // Verificar si el bloque del JPEG se solapa con el área de la notificación
    if (x + w > notif_x_min && x < notif_x_max && y + h > notif_y_min &&
        y < notif_y_max) {
      uint16_t *temp_bitmap = (uint16_t *)malloc(w * h * sizeof(uint16_t));
      if (temp_bitmap != NULL) {
        memcpy(temp_bitmap, bitmap, w * h * sizeof(uint16_t));

        for (int16_t by = 0; by < h; by++) {
          int16_t py = y + by;
          if (py >= notif_y_min && py <= notif_y_max) {
            for (int16_t bx = 0; bx < w; bx++) {
              int16_t px = x + bx;
              if (px >= notif_x_min && px <= notif_x_max) {
                // Difuminado espacial 3x3 (Box Blur) sin retroalimentación
                // leyendo del búfer temporal
                uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
                for (int dy = -1; dy <= 1; dy++) {
                  int ny = constrain(by + dy, 0, h - 1);
                  for (int dx = -1; dx <= 1; dx++) {
                    int nx = constrain(bx + dx, 0, w - 1);
                    uint16_t c = temp_bitmap[ny * w + nx];
                    sum_r += (c >> 11) & 0x1F;
                    sum_g += (c >> 5) & 0x3F;
                    sum_b += c & 0x1F;
                  }
                }
                uint16_t avg_r = sum_r / 9;
                uint16_t avg_g = sum_g / 9;
                uint16_t avg_b = sum_b / 9;

                // Mezcla neutra efecto cristal (75% fondo difuminado + 25%
                // blanco escarchado)
                uint16_t blend_r = (avg_r * 3 + 31) >> 2;
                uint16_t blend_g = (avg_g * 3 + 63) >> 2;
                uint16_t blend_b = (avg_b * 3 + 31) >> 2;

                bitmap[by * w + bx] =
                    (blend_r << 11) | (blend_g << 5) | blend_b;
              }
            }
          }
        }
        free(temp_bitmap);
      }
    }
  }

  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// =========================================================================
// Carga y Guardado del Token de Dispositivo en Memoria Flash (LittleFS)
// =========================================================================
bool cargarTokenFS() {
  if (!fsInicializada)
    return false;
  if (LittleFS.exists("/token.txt")) {
    File archivo = LittleFS.open("/token.txt", FILE_READ);
    if (archivo) {
      String tokenStr = archivo.readStringUntil('\n');
      tokenStr.trim();
      archivo.close();
      if (tokenStr.length() > 0) {
        strncpy(device_token, tokenStr.c_str(), 64);
        device_token[64] = '\0';
        Serial.printf("Token de dispositivo cargado desde Flash: %s\n",
                      device_token);
        return true;
      }
    }
  }
  Serial.println("Token de dispositivo no encontrado en almacenamiento Flash.");
  return false;
}

void guardarTokenFS(String token) {
  if (!fsInicializada)
    return;
  File archivo = LittleFS.open("/token.txt", FILE_WRITE);
  if (archivo) {
    archivo.println(token);
    archivo.close();
    strncpy(device_token, token.c_str(), 64);
    device_token[64] = '\0';
    Serial.printf("Token de dispositivo guardado con exito: %s\n",
                  device_token);
  }
}

// =========================================================================
// Inicialización y Escaneo de la Memoria Interna (LittleFS)
// =========================================================================
bool iniciarFS() {
  Serial.println("Inicializando sistema de archivos interno LittleFS...");
  if (LittleFS.begin(true)) {
    Serial.println("¡Memoria interna LittleFS montada con éxito!");
    fsInicializada = true;

    // Crear carpeta de fotos si no existe
    if (!LittleFS.exists("/fotos")) {
      LittleFS.mkdir("/fotos");
    }
    return true;
  } else {
    Serial.println("Error: No se pudo montar el sistema de archivos interno.");
    fsInicializada = false;
    return false;
  }
}

void escanearFotosEnFS() {
  if (!fsInicializada)
    return;

  listaFotoIds.clear();
  String dirPath = "/fotos";
  if (active_galeria_id.length() > 0) {
    dirPath = "/fotos/gal_" + active_galeria_id;
    if (!LittleFS.exists(dirPath)) {
      LittleFS.mkdir(dirPath);
    }
  }

  File root = LittleFS.open(dirPath);
  if (!root || !root.isDirectory()) {
    Serial.printf("Error al abrir directorio %s\n", dirPath.c_str());
    return;
  }

  File archivo = root.openNextFile();
  while (archivo) {
    if (!archivo.isDirectory()) {
      String nombre = archivo.name();

      // Extraer el ID de la foto (nombre del archivo antes del .jpg)
      int indexSlash = nombre.lastIndexOf('/');
      int indexDot = nombre.lastIndexOf('.');
      String idStr = nombre.substring(indexSlash + 1, indexDot);
      int idVal = idStr.toInt();

      if (idVal > 0) {
        listaFotoIds.push_back(idVal);
      }
    }
    archivo = root.openNextFile();
  }

  // Ordenar los IDs de menor a mayor para mantener la secuencia temporal
  std::sort(listaFotoIds.begin(), listaFotoIds.end());

  Serial.printf("Escaneo completado. Se encontraron %d fotos locales en el "
                "directorio: %s\n",
                listaFotoIds.size(), dirPath.c_str());
}

// =========================================================================
// Mostrar Imagen desde Almacenamiento Local (LittleFS)
// =========================================================================
bool mostrarImagenLocalPorId(int id) {
  if (!fsInicializada)
    return false;

  String ruta = getRutaFoto(id);
  Serial.printf("Cargando imagen desde almacenamiento local: %s\n",
                ruta.c_str());

  File archivo = LittleFS.open(ruta, FILE_READ);
  if (!archivo) {
    Serial.println("Error: No se pudo abrir el archivo de la memoria interna.");
    return false;
  }

  size_t size = archivo.size();
  if (size == 0) {
    archivo.close();
    return false;
  }

  // Reservar memoria RAM temporal para decodificar
  uint8_t *buffer = (uint8_t *)malloc(size);
  if (buffer == NULL) {
    Serial.println("Error: RAM insuficiente para cargar archivo desde la "
                   "memoria interna.");
    archivo.close();
    return false;
  }

  archivo.read(buffer, size);
  archivo.close();

  tft.fillScreen(TFT_BLACK);

  // Decodificar el JPEG desde el buffer de memoria
  uint32_t tiempoInicio = millis();
  int resultado = TJpgDec.drawJpg(0, 0, buffer, size);
  uint32_t tiempoTotal = millis() - tiempoInicio;

  free(buffer);

  if (resultado == 0) {
    Serial.printf("Imagen local %d renderizada con éxito en %d ms.\n", id,
                  tiempoTotal);
    idFotoActual = id; // Registrar que esta es la foto activa en pantalla

    // Actualizar el índice de visualización local
    auto it = std::find(listaFotoIds.begin(), listaFotoIds.end(), id);
    if (it != listaFotoIds.end()) {
      indiceFotoLocal = std::distance(listaFotoIds.begin(), it);
    }

    // Dibujar notificación si está activa
    if (mostrarNotificacionActiva) {
      dibujarNotificacionEsquina();
    }
    return true;
  } else {
    Serial.printf("Error al decodificar JPEG. Código: %d\n", resultado);
    return false;
  }
}

// =========================================================================
// Refrescar la pantalla según el modo y las fotos guardadas
// =========================================================================
void refrescarPantallaActual() {
  if (!listaFotoIds.empty()) {
    if (indiceFotoLocal < 0 || indiceFotoLocal >= listaFotoIds.size()) {
      indiceFotoLocal =
          listaFotoIds.size() - 1; // Por defecto mostrar la última
    }
    mostrarImagenLocalPorId(listaFotoIds[indiceFotoLocal]);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_MAGENTA);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Listo para recibir fotos", tft.width() / 2,
                   tft.height() / 2 - 10, 4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Esperando señal...", tft.width() / 2, tft.height() / 2 + 25,
                   2);
  }
}

// =========================================================================
// Descargar desde S3 y Guardar en LittleFS (Usa buffer de 512B para no agotar
// RAM)
// =========================================================================
bool descargarYGuardarImagenFS(String url, int id) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  String rutaDestino = getRutaFoto(id);
  if (active_galeria_id.length() > 0) {
    String dirPath = "/fotos/gal_" + active_galeria_id;
    if (!LittleFS.exists(dirPath)) {
      LittleFS.mkdir(dirPath);
    }
  }
  Serial.printf("Descargando de internet y guardando localmente: %s\n",
                rutaDestino.c_str());

  HTTPClient http;
  http.setTimeout(15000); // 15 segundos de timeout para descargas
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int longitudTotal = http.getSize();
    if (longitudTotal <= 0) {
      Serial.println("Error: Tamaño de imagen inválido.");
      http.end();
      return false;
    }

    // Crear el archivo en la memoria interna
    File archivo = LittleFS.open(rutaDestino, FILE_WRITE);
    if (!archivo) {
      Serial.println(
          "Error: No se pudo crear el archivo en la memoria interna.");
      http.end();
      return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer_temp[512]; // Buffer muy pequeño para streaming
    int bytesLeidos = 0;
    unsigned long tiempoInicio = millis();

    // Escribir de forma secuencial en bloques de 512 bytes
    // Esto consume casi 0 de RAM, garantizando descargas exitosas y estables
    while (http.connected() && bytesLeidos < longitudTotal) {
      size_t disponible = stream->available();
      if (disponible) {
        int c = stream->readBytes(buffer_temp,
                                  std::min(disponible, sizeof(buffer_temp)));
        archivo.write(buffer_temp, c);
        bytesLeidos += c;
      }
      // Evitar bloqueos infinitos si la red falla
      if (millis() - tiempoInicio > 15000 && bytesLeidos == 0) {
        Serial.println("Error: Timeout en la descarga.");
        break;
      }
    }

    archivo.close();
    http.end();

    if (bytesLeidos == longitudTotal) {
      Serial.printf(
          "¡Imagen %d descargada y guardada localmente exitosamente!\n", id);

      // Actualizar nuestra lista en memoria
      if (std::find(listaFotoIds.begin(), listaFotoIds.end(), id) ==
          listaFotoIds.end()) {
        listaFotoIds.push_back(id);
        std::sort(listaFotoIds.begin(), listaFotoIds.end());
      }
      return true;
    } else {
      Serial.println("Error: Descarga incompleta. Eliminando archivo dañado.");
      LittleFS.remove(rutaDestino);
      return false;
    }
  } else {
    Serial.printf("Error HTTP: %d\n", httpCode);
    http.end();
    return false;
  }
}

// =========================================================================
// Descargar, guardar en la memoria local y mostrar inmediatamente
// =========================================================================
bool descargarYMostrarImagen(String url, int id) {
  if (fsInicializada) {
    String rutaLocal = getRutaFoto(id);

    // =================================================================
    // OPTIMIZACIÓN CRÍTICA: EVIDAR DESCARGAS REPETIDAS
    // Si el archivo de imagen ya existe físicamente en la memoria local,
    // no consumimos ancho de banda ni escribimos en la flash; lo mostramos de
    // inmediato.
    // =================================================================
    if (LittleFS.exists(rutaLocal)) {
      Serial.printf("La imagen %d ya existe en la memoria interna. Cargando "
                    "localmente...\n",
                    id);
      return mostrarImagenLocalPorId(id);
    }

    // Si no existe, proceder con la descarga normal
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Descargando imagen...", tft.width() / 2, tft.height() / 2,
                   4);

    if (descargarYGuardarImagenFS(url, id)) {
      return mostrarImagenLocalPorId(id);
    }
  } else {
    // Fallback en caso de fallo crítico en la memoria flash (descarga en RAM)
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Descargando imagen...", tft.width() / 2, tft.height() / 2,
                   4);

    Serial.println("Advertencia: LittleFS no montada. Descargando "
                   "temporalmente en RAM...");
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();
      uint8_t *ram_buf = (uint8_t *)malloc(len);
      if (ram_buf != NULL) {
        WiFiClient *stream = http.getStreamPtr();
        stream->readBytes(ram_buf, len);
        http.end();
        tft.fillScreen(TFT_BLACK);
        TJpgDec.drawJpg(0, 0, ram_buf, len);
        free(ram_buf);
        return true;
      }
    }
    http.end();
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.drawString("Error de descarga", tft.width() / 2, tft.height() / 2, 4);
  return false;
}

// =========================================================================
// Calibración y Carga del Panel Táctil (Touch)
// =========================================================================
void calibrarPantalla() {
  uint16_t calData[5];

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Calibrando Pantalla Táctil", tft.width() / 2,
                 tft.height() / 2 - 40, 4);
  tft.drawString("Toque las esquinas indicadas", tft.width() / 2,
                 tft.height() / 2, 2);
  tft.drawString("con un lapiz o con su dedo", tft.width() / 2,
                 tft.height() / 2 + 30, 2);

  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

  if (fsInicializada) {
    File archivo = LittleFS.open("/calibracion.txt", FILE_WRITE);
    if (archivo) {
      for (int i = 0; i < 5; i++) {
        archivo.println(calData[i]);
      }
      archivo.close();
      Serial.println("Calibración guardada en memoria interna.");
    }
  }

  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);
}

void cargarCalibracion() {
  uint16_t calData[5];
  bool calDataValido = false;

  if (fsInicializada && LittleFS.exists("/calibracion.txt")) {
    File archivo = LittleFS.open("/calibracion.txt", FILE_READ);
    if (archivo) {
      int i = 0;
      while (archivo.available() && i < 5) {
        calData[i] = archivo.readStringUntil('\n').toInt();
        i++;
      }
      archivo.close();
      if (i == 5) {
        calDataValido = true;
        Serial.println("Datos de calibración leídos de la memoria interna.");
      }
    }
  }

  if (calDataValido) {
    tft.setTouch(calData);
  } else {
    calibrarPantalla();
  }
}

// =========================================================================
// Notificación Temporal Visual en Pantalla (Diseño de cristal elegante)
// =========================================================================
// =========================================================================
// Cargar y Guardar Intervalo de Carrusel de forma persistente (LittleFS)
// =========================================================================
void guardarIntervaloCarrusel() {
  if (!fsInicializada)
    return;
  File archivo = LittleFS.open("/carrusel_interval.txt", FILE_WRITE);
  if (archivo) {
    archivo.println(intervaloCarrusel / 1000);
    archivo.close();
    Serial.printf("Intervalo de carrusel guardado en Flash: %d segundos\n",
                  intervaloCarrusel / 1000);
  }
}

void cargarIntervaloCarrusel() {
  if (fsInicializada && LittleFS.exists("/carrusel_interval.txt")) {
    File archivo = LittleFS.open("/carrusel_interval.txt", FILE_READ);
    if (archivo) {
      String valStr = archivo.readStringUntil('\n');
      archivo.close();
      int val = valStr.toInt();
      if (val >= 1 && val <= 60) { // Validar rango seguro de 1 a 60 segundos
        intervaloCarrusel = val * 1000;
        Serial.printf(
            "Intervalo de carrusel cargado desde flash: %d segundos\n", val);
      }
    }
  }
}

// =========================================================================
// Dibujar Corazón Relleno (Decoración Estética)
// =========================================================================
void dibujarCorazon(int16_t x, int16_t y, int16_t r, uint16_t color) {
  tft.fillCircle(x - r, y, r, color);
  tft.fillCircle(x + r, y, r, color);
  tft.fillTriangle(x - 2 * r, y, x + 2 * r, y, x, y + 2.2 * r, color);
}

// =========================================================================
// Dibujar Notificación en Esquina Superior Derecha (Estilo Glassmorphic)
// =========================================================================
void dibujarNotificacionEsquina() {
  if (!mostrarNotificacionActiva)
    return;

  // Coordenadas: X=240 a 470 (ancho 230), Y=10 a 65 (alto 55)
  // Si no hay fotos de fondo cargadas, dibujamos un fondo sólido para
  // legibilidad. Si hay fotos, no dibujamos fondo aquí porque tft_output ya
  // pintó el efecto "glass" mezclando los píxeles en caliente.
  if (listaFotoIds.empty()) {
    tft.fillRoundRect(240, 10, 230, 55, 8, 0x3006);
  }

  // Dibujar bordes elegantes de cristal
  tft.drawRoundRect(240, 10, 230, 55, 8, 0xFD1A);
  tft.drawRoundRect(241, 11, 228, 53, 7, 0xF97F);

  // Dibujar textos con fondo transparente (flotando sobre el vidrio esmerilado)
  // Se utilizan colores oscuros de alto contraste (violeta oscuro y magenta
  // fuerte) para garantizar legibilidad excelente
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(0x3006); // Violeta oscuro para "Subido por:" o el modo
  tft.drawString(textoNotificacion1, 255, 18, 2);
  tft.setTextColor(
      0xD012); // Magenta fuerte para el nombre del usuario o el valor
  tft.drawString(textoNotificacion2, 255, 38, 2);
}

// =========================================================================
// Vista de Ajustes/Configuraciones de Carrusel
// =========================================================================
void dibujarMenuConfiguraciones(bool redibujarTodo) {
  menuConfiguracionesAbierto = true;
  int segundos = intervaloCarrusel / 1000;

  if (redibujarTodo) {
    // 1. Limpiar pantalla completa con fondo rosa pastel (sin recargas molestas
    // de fotos!)
    tft.fillScreen(0xFDF7);
    tft.drawRoundRect(20, 20, 440, 280, 15, 0xF97F); // Borde rosa
    tft.drawRoundRect(22, 22, 436, 276, 13, 0xD012); // Borde magenta

    // Título con corazones
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x6003); // Texto magenta oscuro
    tft.drawString("AJUSTES CARRUSEL", 240, 75, 4);
    dibujarCorazon(105, 75, 5, 0xFD1A);
    dibujarCorazon(375, 75, 5, 0xFD1A);

    // Botón Volver (Magenta)
    // Coordenadas: X=170-310, Y=225-265
    tft.fillRoundRect(170, 225, 140, 40, 8, 0xD012);
    tft.drawRoundRect(170, 225, 140, 40, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Volver", 240, 245, 2);
  }

  // 2. Área interactiva del Slider
  // Borramos el área para redibujar de forma muy veloz sin parpadeos sobre el
  // fondo rosa pastel (0xFDF7)
  tft.fillRect(50, 105, 380, 90, 0xFDF7);

  // Etiqueta del valor actual
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0x6003); // Magenta oscuro
  char textoValor[30];
  sprintf(textoValor, "Intervalo: %d seg", segundos);
  tft.drawString(textoValor, 240, 120, 2);

  // Pista del deslizador (Track)
  tft.fillRoundRect(100, 150, 280, 8, 4, 0xE73F); // Gris rosa suave
  tft.drawRoundRect(100, 150, 280, 8, 4, 0xF97F); // Contorno rosa

  // Relleno de avance activo
  int sliderX = 100 + (segundos * 280 / 60);
  if (sliderX > 100) {
    tft.fillRoundRect(100, 150, sliderX - 100, 8, 4, 0xFD1A); // Rosa neón
  }

  // Perilla del deslizador (Knob)
  tft.fillCircle(sliderX, 154, 12, 0xFD1A);   // Exterior rosa fuerte
  tft.fillCircle(sliderX, 154, 8, TFT_WHITE); // Interior blanco
}

// =========================================================================
// Procesar deslizamiento táctil (Arrastre) en el Slider en tiempo real
// =========================================================================
void procesarArrastreSlider(uint16_t x, uint16_t y) {
  if (y >= 120 && y <= 185) {
    int xClamped = constrain(x, 100, 380);
    int segundos = (xClamped - 100) * 60 / 280;

    // Evitar cambios a 0 segundos (mínimo seguro: 1 segundo)
    if (segundos < 1)
      segundos = 1;

    unsigned long nuevoIntervalo = segundos * 1000;
    if (nuevoIntervalo != intervaloCarrusel) {
      intervaloCarrusel = nuevoIntervalo;
      dibujarMenuConfiguraciones(false); // Redibujar fluidamente
    }
  }
}

void procesarTouchConfiguraciones(uint16_t x, uint16_t y) {
  // 1. Click directo en la barra del slider
  if (y >= 120 && y <= 185 && x >= 100 && x <= 380) {
    procesarArrastreSlider(x, y);
  }
  // 2. Botón Volver (X=170-310, Y=225-265)
  else if (x >= 170 && x <= 310 && y >= 225 && y <= 265) {
    menuConfiguracionesAbierto = false;
    guardarIntervaloCarrusel(); // Guardado persistente
    dibujarMenuModos();         // Volver al menú de selección principal
  }
}

// =========================================================================
// Dibujar Menú de Selección de Modos (Diseño Romántico Pastel y Rápido)
// =========================================================================
void dibujarMenuModos() {
  menuAbierto = true;

  // 1. Limpiar pantalla completa con un fondo rosa pastel muy suave (estilo
  // romántico)
  tft.fillScreen(0xFDF7);                          // Rosa pastel ultra suave
  tft.drawRoundRect(20, 20, 440, 280, 15, 0xF97F); // Borde rosa
  tft.drawRoundRect(22, 22, 436, 276, 13, 0xD012); // Borde magenta

  // 2. Título con corazones
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0x6003); // Magenta oscuro
  tft.drawString("NUESTRO CUADRITO", 240, 65, 4);
  dibujarCorazon(105, 65, 6, 0xFD1A); // Corazón izquierdo
  dibujarCorazon(375, 65, 6, 0xFD1A); // Corazón derecho

  // 3. Botones en cuadrícula moderna
  // Fila 1 Columna 1: Sincro (X=60-220, Y=105-155)
  bool sincroActivo = (modoActual == MODO_SINCRONIZADO);
  tft.fillRoundRect(60, 105, 160, 50, 10, sincroActivo ? 0xD012 : 0xFDB8);
  tft.drawRoundRect(60, 105, 160, 50, 10, sincroActivo ? TFT_WHITE : 0xD012);
  tft.setTextColor(sincroActivo ? TFT_WHITE : 0x6003);
  tft.drawString(sincroActivo ? "❤ Sincro" : "Sincro", 140, 130, 2);

  // Fila 1 Columna 2: Carrusel (X=260-420, Y=105-155)
  bool carruselActivo = (modoActual == MODO_CARRUSEL);
  tft.fillRoundRect(260, 105, 160, 50, 10, carruselActivo ? 0x4B3D : 0xE67D);
  tft.drawRoundRect(260, 105, 160, 50, 10, carruselActivo ? TFT_WHITE : 0x4B3D);
  tft.setTextColor(carruselActivo ? TFT_WHITE : 0x310A);
  tft.drawString(carruselActivo ? "❤ Carrusel" : "Carrusel", 340, 130, 2);

  // Fila 2 Columna 1: Manual (X=60-220, Y=170-220)
  bool manualActivo = (modoActual == MODO_MANUAL);
  tft.fillRoundRect(60, 170, 160, 50, 10, manualActivo ? 0xFD1A : 0xFDB8);
  tft.drawRoundRect(60, 170, 160, 50, 10, manualActivo ? TFT_WHITE : 0xFD1A);
  tft.setTextColor(manualActivo ? TFT_WHITE : 0x6003);
  tft.drawString(manualActivo ? "❤ Manual" : "Manual", 140, 195, 2);

  // Fila 2 Columna 2: Ajustes (X=260-420, Y=170-220)
  tft.fillRoundRect(260, 170, 160, 50, 10, 0xE73F);
  tft.drawRoundRect(260, 170, 160, 50, 10, 0xF97F);
  tft.setTextColor(0x4208);
  tft.drawString("Ajustes", 340, 195, 2);

  // Fila 3 Central: Cerrar (X=170-310, Y=240-275)
  tft.fillRoundRect(170, 240, 140, 35, 8, 0xC000); // Rojo romántico sólido
  tft.drawRoundRect(170, 240, 140, 35, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Cerrar", 240, 257, 2);

  Serial.println("Menú romántico pastel a pantalla completa dibujado.");
}

// =========================================================================
// Procesar los toques físicos dentro del Menú Modal principal
// =========================================================================
void procesarTouchMenu(uint16_t x, uint16_t y) {
  // Botón 1: Sincro (X=60-220, Y=105-155)
  if (x >= 60 && x <= 220 && y >= 105 && y <= 155) {
    modoActual = MODO_SINCRONIZADO;
    menuAbierto = false;
    Serial.println("Modo seleccionado: SINCRONIZADO");

    mostrarNotificacionActiva = true;
    textoNotificacion1 = "Subido por:";
    textoNotificacion2 = usuarioUltimaFoto;
    tiempoNotificacion = millis();
    ocultarNotificacionEnSiguienteFoto = false; // Desaparecerá tras 5s

    sincronizarUltimaFoto();
  }
  // Botón 2: Carrusel (X=260-420, Y=105-155)
  else if (x >= 260 && x <= 420 && y >= 105 && y <= 155) {
    modoActual = MODO_CARRUSEL;
    menuAbierto = false;
    Serial.println("Modo seleccionado: CARRUSEL LOCAL");
    ultimoCambioCarrusel = millis();

    mostrarNotificacionActiva = true;
    textoNotificacion1 = "Modo Carrusel";
    char buf[25];
    sprintf(buf, "Intervalo: %d seg", (int)(intervaloCarrusel / 1000));
    textoNotificacion2 = String(buf);
    tiempoNotificacion = millis();
    ocultarNotificacionEnSiguienteFoto =
        true; // Desaparecerá al cambiar la primera foto

    refrescarPantallaActual();
  }
  // Botón 3: Manual (X=60-220, Y=170-220)
  else if (x >= 60 && x <= 220 && y >= 170 && y <= 220) {
    modoActual = MODO_MANUAL;
    menuAbierto = false;
    Serial.println("Modo seleccionado: CONTROL MANUAL");

    mostrarNotificacionActiva = true;
    textoNotificacion1 = "Modo Manual";
    textoNotificacion2 = "Desliza p/ cambiar";
    tiempoNotificacion = millis();
    ocultarNotificacionEnSiguienteFoto = true; // Desaparecerá al primer gesto

    refrescarPantallaActual();
  }
  // Botón 4: Ajustes (X=260-420, Y=170-220)
  else if (x >= 260 && x <= 420 && y >= 170 && y <= 220) {
    menuAbierto = false;
    dibujarMenuConfiguraciones(true); // Abrir submenú de ajustes
  }
  // Botón Cerrar: (X=170-310, Y=240-275)
  else if (x >= 170 && x <= 310 && y >= 240 && y <= 275) {
    menuAbierto = false;
    Serial.println("Menú cerrado sin cambios.");
    refrescarPantallaActual();
  }
}

// =========================================================================
// Navegación Local de Fotos
// =========================================================================
void mostrarFotoSiguienteLocal() {
  if (listaFotoIds.empty())
    return;

  // Ocultar notificación de modo si está activa al pasar de foto
  if (mostrarNotificacionActiva && ocultarNotificacionEnSiguienteFoto) {
    mostrarNotificacionActiva = false;
  }

  indiceFotoLocal++;
  if (indiceFotoLocal >= listaFotoIds.size()) {
    indiceFotoLocal = 0;
  }

  mostrarImagenLocalPorId(listaFotoIds[indiceFotoLocal]);
}

void mostrarFotoAnteriorLocal() {
  if (listaFotoIds.empty())
    return;

  // Ocultar notificación de modo si está activa al pasar de foto
  if (mostrarNotificacionActiva && ocultarNotificacionEnSiguienteFoto) {
    mostrarNotificacionActiva = false;
  }

  indiceFotoLocal--;
  if (indiceFotoLocal < 0) {
    indiceFotoLocal = listaFotoIds.size() - 1;
  }

  mostrarImagenLocalPorId(listaFotoIds[indiceFotoLocal]);
}

// =========================================================================
// Procesamiento de Pantalla Táctica Inteligente (Gestos y Toques)
// =========================================================================
void procesarTactil() {
  uint16_t x = 0, y = 0;
  bool tocadoAhora = tft.getTouch(&x, &y);

  if (tocadoAhora) {
    if (!estabaTocado) {
      // Comienza el toque
      estabaTocado = true;
      touchStartX = x;
      touchStartY = y;
      touchLastX = x;
      touchLastY = y;
      touchStartTime = millis();
    } else {
      // El usuario arrastra el dedo
      touchLastX = x;
      touchLastY = y;

      // Si el submenú de configuraciones está abierto, procesar slider en
      // caliente
      if (menuConfiguracionesAbierto) {
        procesarArrastreSlider(x, y);
      }
    }
  } else {
    if (estabaTocado) {
      // El usuario levanta el dedo
      estabaTocado = false;
      unsigned long duracion = millis() - touchStartTime;
      int dx = (int)touchLastX - (int)touchStartX;
      int dy = (int)touchLastY - (int)touchStartY;

      // 1. Evaluar si es un gesto de deslizamiento (SWIPE)
      // Se requiere movimiento horizontal de mínimo 60 píxeles y rapidez
      if (abs(dx) > 60 && abs(dx) > abs(dy) && duracion < 800) {
        if (menuAbierto || menuConfiguracionesAbierto) {
          return; // Ignorar gestos de fondo si hay un menú abierto
        }

        if (modoActual == MODO_CARRUSEL || modoActual == MODO_MANUAL) {
          if (dx > 0) {
            Serial.println("Swipe -> Derecha (Foto Anterior)");
            mostrarFotoAnteriorLocal();
            if (modoActual == MODO_CARRUSEL) {
              ultimoCambioCarrusel = millis(); // Reiniciar cronómetro
            }
          } else {
            Serial.println("Swipe -> Izquierda (Foto Siguiente)");
            mostrarFotoSiguienteLocal();
            if (modoActual == MODO_CARRUSEL) {
              ultimoCambioCarrusel = millis(); // Reiniciar cronómetro
            }
          }
        }
      }
      // 2. Evaluar si es un toque directo corto (TAP)
      else if (duracion < 350) {
        Serial.printf("Toque rápido (Tap) en coordenadas X: %d, Y: %d\n",
                      touchStartX, touchStartY);

        if (menuConfiguracionesAbierto) {
          procesarTouchConfiguraciones(touchStartX, touchStartY);
        } else if (menuAbierto) {
          procesarTouchMenu(touchStartX, touchStartY);
        } else {
          // Si los menús están cerrados:
          // Zona superior central (Y < 70, X de 120 a 360) para abrir el menú
          // de modos
          if (touchStartY < 70 && touchStartX > 120 && touchStartX < 360) {
            dibujarMenuModos();
          }
        }
      }
    }
  }
}

// =========================================================================
// Limpia físicamente todas las fotos de la memoria interna
// =========================================================================
void limpiarFotosLocales() {
  Serial.println(
      "Limpiando fotos locales de la memoria flash por cambio de galeria...");
  File root = LittleFS.open("/fotos", FILE_READ);
  if (root && root.isDirectory()) {
    File archivo = root.openNextFile();
    while (archivo) {
      if (!archivo.isDirectory()) {
        String nombre = archivo.name();
        String rutaCompleta = nombre;
        if (!rutaCompleta.startsWith("/fotos/")) {
          rutaCompleta = "/fotos/" + rutaCompleta;
        }
        LittleFS.remove(rutaCompleta);
        Serial.printf("   [FS] Borrada foto local: %s\n", rutaCompleta.c_str());
      }
      archivo = root.openNextFile();
    }
  }
  listaFotoIds.clear();
  indiceFotoLocal = -1;
  idFotoActual = -1;
}

// =========================================================================
// =========================================================================
// Función para reiniciar el dispositivo de fábrica
// =========================================================================
void resetearDispositivo() {
  Serial.println("Reseteando dispositivo a modo fábrica...");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Reseteando...", 240, 160, 4);
  
  if (LittleFS.exists("/token.txt")) {
    LittleFS.remove("/token.txt");
  }
  
  WiFiManager wm;
  wm.resetSettings();
  WiFi.disconnect(true, true);
  
  delay(1000);
  ESP.restart();
}

// =========================================================================
// Muestra pantalla de dispositivo huérfano y espera toque para resetear
// =========================================================================
void mostrarPantallaDesvinculadoYEsperarReset() {
  tft.fillScreen(TFT_ORANGE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Dispositivo Desvinculado", 240, 100, 4);
  tft.drawString("Este marco no pertenece a ninguna cuenta.", 240, 150, 2);
  
  tft.fillRoundRect(80, 200, 320, 60, 10, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("TOCA AQUI PARA RESETEAR", 240, 230, 4);

  while (true) {
    uint16_t x = 0, y = 0;
    if (tft.getTouch(&x, &y)) {
      resetearDispositivo();
    }
    delay(100);
  }
}

// =========================================================================
// Sincroniza catálogo completo de imágenes con el servidor Django (Alta
// Confiabilidad) Descarga fotos nuevas y elimina fotos locales que ya no
// existen en el servidor
// =========================================================================
void sincronizarCatalogoCompleto() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  Serial.println("Sincronizando catalogo completo con el servidor Django...");

  // Dibujar pantalla de progreso con estética romántica pastel
  tft.fillScreen(0xFDF7);                          // Fondo rosa pastel
  tft.drawRoundRect(20, 20, 440, 280, 15, 0xF97F); // Borde rosa
  tft.drawRoundRect(22, 22, 436, 276, 13, 0xD012); // Borde magenta

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0x6003); // Magenta oscuro
  tft.drawString("Sincronizando fotos...", 240, 110, 4);
  tft.drawString("Conectando con la galeria...", 240, 155, 2);

  dibujarCorazon(105, 110, 5, 0xFD1A);
  dibujarCorazon(375, 110, 5, 0xFD1A);

  HTTPClient http;
  http.setTimeout(15000); // 15 segundos de timeout
  http.begin(backend_sync_url);
  http.addHeader("X-Device-Token", device_token);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // ... logic remains unchanged for parsing ...


    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc.is<JsonObject>()) {
      JsonObject obj = doc.as<JsonObject>();
      String galeriaNombre = obj["galeria_nombre"].as<String>();
      String topicGaleria = obj["mqtt_topic"].as<String>();
      String topicDispositivo = obj["mqtt_topic_dispositivo"].as<String>();

      String galeriaId = obj["galeria_id"].as<String>();
      if (galeriaId.length() > 0 && active_galeria_id != galeriaId) {
        active_galeria_id = galeriaId;
        // Al cambiar la galería activa, re-escaneamos la carpeta local
        // correspondiente
        escanearFotosEnFS();
      }

      // Actualizar tópicos de MQTT si cambiaron
      if (topicGaleria.length() > 0 && mqtt_topic_galeria != topicGaleria) {
        if (mqtt_topic_galeria.length() > 0 && mqttClient.connected()) {
          String borrarTopic = mqtt_topic_galeria;
          borrarTopic.replace("nueva_foto", "borrar_foto");
          mqttClient.unsubscribe(mqtt_topic_galeria.c_str());
          mqttClient.unsubscribe(borrarTopic.c_str());
        }
        mqtt_topic_galeria = topicGaleria;
        if (mqttClient.connected()) {
          mqttClient.subscribe(mqtt_topic_galeria.c_str());
          String borrarTopic = mqtt_topic_galeria;
          borrarTopic.replace("nueva_foto", "borrar_foto");
          mqttClient.subscribe(borrarTopic.c_str());
        }
      }

      if (topicDispositivo.length() > 0 &&
          mqtt_topic_dispositivo != topicDispositivo) {
        if (mqtt_topic_dispositivo.length() > 0 && mqttClient.connected()) {
          mqttClient.unsubscribe(mqtt_topic_dispositivo.c_str());
        }
        mqtt_topic_dispositivo = topicDispositivo;
        if (mqttClient.connected()) {
          mqttClient.subscribe(mqtt_topic_dispositivo.c_str());
        }
      }

      JsonArray arr = obj["fotos"].as<JsonArray>();
      int totalFotosServidor = arr.size();
      Serial.printf("Catalogo: %d fotos encontradas en la galeria '%s'.\n",
                    totalFotosServidor, galeriaNombre.c_str());

      std::vector<int> serverIds;
      int descargadasNuevas = 0;

      // 1. Descargar fotos del servidor que no tengamos localmente
      for (int i = 0; i < totalFotosServidor; i++) {
        JsonObject foto = arr[i].as<JsonObject>();
        int id = foto["id"].as<int>();
        String url = foto["imagen"].as<String>();

        if (id > 0 && url.length() > 0) {
          serverIds.push_back(id);
          // Usar getRutaFoto() para verificar en la subcarpeta correcta
          // de la galería activa (evita re-descargar en cada cambio de galería)
          String rutaLocal = getRutaFoto(id);

          if (!LittleFS.exists(rutaLocal)) {
            descargadasNuevas++;

            // Actualizar indicador de progreso en pantalla
            char textoProgreso[60];
            sprintf(textoProgreso, "Descargando: %d de %d...",
                    descargadasNuevas, totalFotosServidor);
            tft.fillRect(45, 140, 390, 40, 0xFDF7); // Borrar área
            tft.drawString(textoProgreso, 240, 155, 2);

            Serial.printf("Sincronizacion: Descargando foto ID %d...\n", id);
            descargarYGuardarImagenFS(url, id);
          } else {
            Serial.printf("Foto ID %d ya existe localmente, omitiendo descarga.\n", id);
          }
        }
      }

      // 2. Eliminar fotos locales huérfanas (que ya no existan en el servidor)
      int eliminadasLocales = 0;
      std::vector<int> copiaLocalIds = listaFotoIds;
      for (int localId : copiaLocalIds) {
        if (std::find(serverIds.begin(), serverIds.end(), localId) ==
            serverIds.end()) {
          // Usar getRutaFoto() para borrar del directorio correcto
          String rutaBorrar = getRutaFoto(localId);
          if (LittleFS.exists(rutaBorrar)) {
            LittleFS.remove(rutaBorrar);
            eliminadasLocales++;
            Serial.printf("Sincronizacion: Eliminando foto huerfana ID %d\n",
                          localId);
          }
        }
      }

      Serial.printf("Catalogo sincronizado. Nuevas: %d, Eliminadas: %d\n",
                    descargadasNuevas, eliminadasLocales);

      // Volver a escanear el disco para tener la lista de IDs actualizada en
      // memoria RAM
      escanearFotosEnFS();

      // Mostrar la última foto sincronizada si el catálogo no está vacío
      if (totalFotosServidor > 0 && modoActual == MODO_SINCRONIZADO) {
        int idUltima = arr[0]["id"].as<int>();
        String urlUltima = arr[0]["imagen"].as<String>();

        String usuarioReq = "Alguien";
        if (arr[0]["usuario"].is<String>()) {
          usuarioReq = arr[0]["usuario"].as<String>();
        }
        usuarioUltimaFoto = usuarioReq;

        mostrarNotificacionActiva = true;
        textoNotificacion1 = "Subido por:";
        textoNotificacion2 = usuarioUltimaFoto;
        tiempoNotificacion = millis();
        ocultarNotificacionEnSiguienteFoto = false;

        descargarYMostrarImagen(urlUltima, idUltima);
      } else {
        refrescarPantallaActual();
      }
    } else {
      Serial.println("Error: Catalogo JSON invalido o vacio.");
    }
  } else if (httpCode == 404 || httpCode == 401) {
    Serial.println("Dispositivo desvinculado (Error 404/401). Mostrando pantalla de reseteo...");
    http.end();
    mostrarPantallaDesvinculadoYEsperarReset();
    return; // Nunca debería llegar aquí por el bucle infinito
  } else {
    Serial.printf("Error al obtener catalogo. Codigo HTTP: %d\n", httpCode);
  }

  http.end();
}

// =========================================================================
// Sincroniza y descarga la última foto cargada en el backend de Django
// =========================================================================
// =========================================================================
// Sincroniza y descarga la última foto cargada en el backend de Django
// =========================================================================
void sincronizarUltimaFoto() {
  // Resetear el ID de foto actual para que la guardia del loop
  // no descarte el mensaje MQTT retenido que vamos a solicitar
  idFotoActual = -1;

  // Forzar al broker MQTT a re-entregar el mensaje 'retain' (la última foto
  // activa). Un unsub+sub hace que el broker re-envíe automáticamente
  // el último mensaje retenido del topic, sin necesidad de llamar al servidor.
  if (mqttClient.connected() && mqtt_topic_galeria.length() > 0) {
    mqttClient.unsubscribe(mqtt_topic_galeria.c_str());
    delay(50);
    mqttClient.subscribe(mqtt_topic_galeria.c_str());
    Serial.println("Re-suscrito al topic para obtener la foto activa del broker.");
  } else {
    // Sin MQTT, mostramos la última foto local como fallback
    if (!listaFotoIds.empty()) {
      indiceFotoLocal = listaFotoIds.size() - 1;
    }
    refrescarPantallaActual();
  }
}

// =========================================================================
// Callback de MQTT: NO BLOQUEANTE (Solo activa banderas)
// =========================================================================
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  char mensaje[length + 1];
  memcpy(mensaje, payload, length);
  mensaje[length] = '\0';

  Serial.printf("Mensaje MQTT recibido en [%s]: %s\n", topic, mensaje);

  String topicStr = String(topic);

  // 1. EVENTO: NUEVA FOTO (Canal dinámico de la galería)
  if (topicStr == mqtt_topic_galeria) {
    String urlCandidata = "";
    int idCandidato = -1;
    String usuarioCandidato = "Alguien";

    if (mensaje[0] == '{') {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, mensaje);
      if (!error && doc["url"].is<String>() && doc["id"].is<int>()) {
        urlCandidata = doc["url"].as<String>();
        idCandidato = doc["id"].as<int>();
        if (doc["usuario"].is<String>()) {
          usuarioCandidato = doc["usuario"].as<String>();
        }
      }
    }

    if (idCandidato > 0 && (urlCandidata.startsWith("http://") ||
                            urlCandidata.startsWith("https://"))) {
      nuevaUrlImagen = urlCandidata;
      nuevoIdImagen = idCandidato;
      usuarioUltimaFoto = usuarioCandidato;
      tengoNuevaImagen = true; // Activar bandera
    }
  }
  // 2. EVENTO: BORRAR FOTO (Canal dinámico de la galería)
  else if (topicStr.endsWith("borrar_foto")) {
    int idABorrar = -1;
    if (mensaje[0] == '{') {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, mensaje);
      if (!error && doc["id"].is<int>()) {
        idABorrar = doc["id"].as<int>();
      }
    }

    if (idABorrar > 0) {
      idFotoABorrar = idABorrar;
      tengoFotoBorrada = true;
    }
  }
  // 3. EVENTO: COMANDO DE DISPOSITIVO (Cambio de galería activa en caliente)
  else if (topicStr == mqtt_topic_dispositivo) {
    if (mensaje[0] == '{') {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, mensaje);
      if (!error) {
        String accion = doc["accion"].as<String>();
        if (accion == "cambiar_galeria") {
          String nuevaGaleriaId = doc["galeria_id"].as<String>();
          String nuevaGaleriaNombre = doc["nombre"].as<String>();
          Serial.printf("Comando de cambio de galeria recibido: %s (%s)\n",
                        nuevaGaleriaNombre.c_str(), nuevaGaleriaId.c_str());

          // Mostrar notificación flotante de cambio de galería
          mostrarNotificacionActiva = true;
          textoNotificacion1 = "Galeria Cambiada:";
          textoNotificacion2 = nuevaGaleriaNombre;
          tiempoNotificacion = millis();
          ocultarNotificacionEnSiguienteFoto = false;

          // En lugar de borrar fotos, simplemente cambiamos de carpeta y
          // escaneamos
          active_galeria_id = nuevaGaleriaId;
          escanearFotosEnFS();

          // Forzar sincronización completa de la nueva galería
          necesitaSincronizarCatalogo = true;
        }
      }
    }
  }
}

// =========================================================================
// Conexión y Reconexión a WiFi y MQTT (Portal Cautivo mediante WiFiManager)
// =========================================================================
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.println("Iniciando WiFiManager...");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_MAGENTA);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Iniciando WiFi...", tft.width() / 2, tft.height() / 2 - 20,
                 4);
  tft.drawString("Espere por favor", tft.width() / 2, tft.height() / 2 + 20, 2);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  Serial.println("Si no se conecta a una red guardada, busque la red WiFi:");
  Serial.println(">>> NuestroCuadrito-Config <<<");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_MAGENTA);
  tft.drawString("Configure el WiFi", tft.width() / 2, tft.height() / 2 - 30,
                 4);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Conectese a la red:", tft.width() / 2, tft.height() / 2, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("NuestroCuadrito-Config", tft.width() / 2,
                 tft.height() / 2 + 30, 4);

  bool resultado = wm.autoConnect("NuestroCuadrito-Config");

  if (!resultado) {
    Serial.println("Fallo en la conexión o timeout del portal.");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.drawString("Error de Conexion", tft.width() / 2, tft.height() / 2 - 20,
                   4);
    tft.drawString("Reiniciando dispositivo...", tft.width() / 2,
                   tft.height() / 2 + 25, 2);
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("¡WiFi Conectado con éxito!");
    Serial.print("IP Asignada: ");
    Serial.println(WiFi.localIP());

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.drawString("WiFi Conectado!", tft.width() / 2, tft.height() / 2 - 10,
                   4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(WiFi.localIP().toString(), tft.width() / 2,
                   tft.height() / 2 + 25, 2);
    delay(1500);
  }
}

// =========================================================================
// Flujo de Emparejamiento Inteligente por Código PIN de 6 dígitos
// =========================================================================
void realizarVinculacionInteligente() {
  String mac = WiFi.macAddress();
  Serial.printf("Iniciando flujo de vinculacion. MAC: %s\n", mac.c_str());

  // Dibujar pantalla de espera inicial para dar retroalimentación inmediata
  tft.fillScreen(0xFDF7);                          // Fondo rosa pastel
  tft.drawRoundRect(20, 20, 440, 280, 15, 0xF97F); // Borde rosa
  tft.drawRoundRect(22, 22, 436, 276, 13, 0xD012); // Borde magenta

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0x6003); // Magenta oscuro
  tft.drawString("VINCULAR CUADRO", 240, 60, 4);
  dibujarCorazon(105, 60, 5, 0xFD1A);
  dibujarCorazon(375, 60, 5, 0xFD1A);

  tft.setTextColor(TFT_BLACK);
  tft.drawString("Conectando con el servidor Django...", 240, 130, 2);
  tft.setTextColor(0x7D5E); // Violeta suave
  tft.drawString("Obteniendo codigo PIN...", 240, 170, 2);

  String pin = "";

  // 1. Obtener código PIN de Django
  while (pin.length() == 0) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      continue;
    }

    HTTPClient http;
    http.begin(backend_vincular_codigo_url);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"mac\":\"" + mac + "\"}";
    int httpCode = http.POST(jsonPayload);

    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, response);
      if (!err && doc["codigo"].is<String>()) {
        pin = doc["codigo"].as<String>();
        Serial.printf("Codigo PIN de vinculacion obtenido: %s\n", pin.c_str());
      }
    } else {
      Serial.printf(
          "Fallo al obtener PIN de Django. Codigo HTTP: %d. Reintentando...\n",
          httpCode);

      // Mostrar diagnóstico en pantalla para que el usuario sepa qué IP está
      // fallando
      tft.fillRect(30, 100, 420, 160, 0xFDF7); // Limpiar zona de texto
      tft.setTextColor(TFT_RED);
      tft.drawString("Error de conexion con el servidor", 240, 120, 2);

      tft.setTextColor(TFT_BLACK);
      tft.drawString("Intentando conectar a:", 240, 150, 2);

      // Si la URL es muy larga, acortarla para el string en pantalla
      String urlCortada = backend_vincular_codigo_url;
      if (urlCortada.length() > 40) {
        urlCortada = urlCortada.substring(0, 38) + "...";
      }
      tft.setTextColor(0x6003);
      tft.drawString(urlCortada, 240, 180, 2);

      tft.setTextColor(0x7D5E);
      tft.drawString("Verifica que Django este corriendo en esa IP", 240, 210,
                     2);
    }
    http.end();
    if (pin.length() == 0)
      delay(5000); // Esperar antes de reintentar
  }

  // 2. Mostrar código PIN en la pantalla TFT con estética romántica rosa
  tft.fillScreen(0xFDF7);                          // Fondo rosa pastel
  tft.drawRoundRect(20, 20, 440, 280, 15, 0xF97F); // Borde rosa
  tft.drawRoundRect(22, 22, 436, 276, 13, 0xD012); // Borde magenta

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0x6003); // Magenta oscuro
  tft.drawString("VINCULAR CUADRO", 240, 60, 4);

  dibujarCorazon(105, 60, 5, 0xFD1A);
  dibujarCorazon(375, 60, 5, 0xFD1A);

  tft.setTextColor(TFT_BLACK);
  tft.drawString("Ingresa este codigo PIN en tu app web:", 240, 115, 2);

  // Dibujar el PIN con letra muy grande y llamativa
  tft.setTextColor(0xD012);         // Magenta neón
  tft.drawString(pin, 240, 175, 6); // Fuente gigante

  tft.setTextColor(0x7D5E); // Violeta suave
  tft.drawString("Esperando vinculacion...", 240, 240, 2);

  // 3. Loop de polling esperando que el usuario confirme el PIN en la web
  bool vinculado = false;
  unsigned long ultimoIntentoPoll = 0;

  while (!vinculado) {
    yield();
    delay(10);

    unsigned long ahora = millis();
    if (ahora - ultimoIntentoPoll > 5000) {
      ultimoIntentoPoll = ahora;

      if (WiFi.status() != WL_CONNECTED)
        continue;

      HTTPClient http;
      String urlConParam = String(backend_vincular_estado_url) + "?mac=" + mac;
      http.begin(urlConParam);

      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);

        if (!err && doc["estado"].is<String>()) {
          String estado = doc["estado"].as<String>();
          if (estado == "vinculado" && doc["token"].is<String>()) {
            String token_final = doc["token"].as<String>();
            Serial.println("Dispositivo vinculado con exito por el usuario!");

            // Guardar el token en la flash
            guardarTokenFS(token_final);
            vinculado = true;

            // Dibujar pantalla de éxito momentánea
            tft.fillScreen(0xFDF7);
            tft.setTextColor(0x27AE); // Verde
            tft.drawString("VINCULADO!", 240, 120, 4);
            tft.setTextColor(TFT_BLACK);
            tft.drawString("Cargando tus fotos compartidas...", 240, 170, 2);
            delay(2000);
          } else {
            Serial.println(
                "Estado de vinculacion: Esperando confirmacion en la web...");
          }
        }
      } else {
        Serial.printf("Error de conexion al verificar vinculacion: %d\n",
                      httpCode);
      }
      http.end();
    }
  }
}

void asegurarConexionMQTT() {
  if (mqttClient.connected())
    return;

  unsigned long ahora = millis();
  if (ahora - ultimoIntentoReconexion > 5000) {
    ultimoIntentoReconexion = ahora;

    Serial.print("Intentando conectar al Broker MQTT seguro (HiveMQ Cloud)...");
    String clienteId = "CuadritoCliente-" + WiFi.macAddress();

    bool conectado = false;
    if (strlen(mqtt_user) > 0) {
      conectado = mqttClient.connect(clienteId.c_str(), mqtt_user, mqtt_pass);
    } else {
      conectado = mqttClient.connect(clienteId.c_str());
    }

    if (conectado) {
      Serial.println(" ¡Conectado con éxito!");

      if (mqtt_topic_galeria.length() > 0) {
        mqttClient.subscribe(mqtt_topic_galeria.c_str());
        String borrarTopic = mqtt_topic_galeria;
        borrarTopic.replace("nueva_foto", "borrar_foto");
        mqttClient.subscribe(borrarTopic.c_str());
        Serial.printf("Suscrito a tópicos de galería: %s y %s\n",
                      mqtt_topic_galeria.c_str(), borrarTopic.c_str());
      }
      if (mqtt_topic_dispositivo.length() > 0) {
        mqttClient.subscribe(mqtt_topic_dispositivo.c_str());
        Serial.printf("Suscrito a tópico de dispositivo: %s\n",
                      mqtt_topic_dispositivo.c_str());
      }
    } else {
      Serial.printf(" Falló. Código de error: %d. Reintentando en 5s.\n",
                    mqttClient.state());
    }
  }
}

// =========================================================================
// Setup principal del Sistema
// =========================================================================
void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Iniciando Sistema Nuestro Cuadrito ===");

  // 1. Inicializar la pantalla física
  tft.begin();
  tft.setRotation(1);     // Rotación horizontal (480x320 píxeles)
  tft.setSwapBytes(true); // Corrección de colores distorsionados para JPEG
  tft.fillScreen(TFT_BLACK);

  // 2. Encender la retroiluminación (Backlight) en el GPIO 27
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  Serial.println("Pantalla inicializada y Backlight encendido.");

  // 3. Configurar el decodificador JPEG
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  // 4. Configurar cliente MQTT seguro con TLS
  espClientSecure.setInsecure(); // Omitir validación de cadena de certificados
                                 // CA, pero mantiene encriptación completa
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // 5. Conectar a la red local
  conectarWiFi();

  // 6. Inicializar y Escanear Memoria Flash Interna (LittleFS)
  if (iniciarFS()) {
    cargarCalibracion();
    cargarIntervaloCarrusel(); // Cargar el intervalo personalizado del usuario

    // Cargar el token o iniciar el flujo de vinculación inteligente si no
    // existe
    if (!cargarTokenFS()) {
      realizarVinculacionInteligente();
    }

    // Escanear fotos locales de la galería activa
    escanearFotosEnFS();
  }

  // 7. Sincronizar catálogo completo de fotos con el servidor y mostrar la
  // última
  sincronizarCatalogoCompleto();
}

// =========================================================================
// Bucle de Ejecución Principal (Loop)
// =========================================================================
void loop() {
  // Mantener la conectividad de red y MQTT
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    necesitaSincronizarCatalogo =
        true; // Marcar para sincronización al recuperar conexión
  } else {
    asegurarConexionMQTT();
    // Si recuperamos la conexión, realizar una sincronización completa en
    // segundo plano
    if (necesitaSincronizarCatalogo) {
      necesitaSincronizarCatalogo = false;
      sincronizarCatalogoCompleto();
    }
  }

  // Procesar MQTT de forma rápida y no bloqueante
  mqttClient.loop();

  // 1. PROCESAMIENTO DE BANDERAS DE DESCARGA (Fuera del callback)
  if (tengoNuevaImagen) {
    tengoNuevaImagen = false;

    if (nuevoIdImagen > 0) {
      if (modoActual == MODO_SINCRONIZADO) {
        // En modo Sincro SIEMPRE procesamos la foto, incluso si el ID coincide
        // con idFotoActual, porque la pantalla puede estar mostrando otra cosa
        // (ej: se estaba en modo manual y se acaba de volver al Sincro)
        mostrarNotificacionActiva = true;
        textoNotificacion1 = "Subido por:";
        textoNotificacion2 = usuarioUltimaFoto;
        tiempoNotificacion = millis();
        ocultarNotificacionEnSiguienteFoto = false;

        descargarYMostrarImagen(nuevaUrlImagen, nuevoIdImagen);
      } else {
        // En modos Manual/Carrusel: descargar en segundo plano sin mostrar
        // Sólo ignorar si ya tenemos esa foto en pantalla
        if (nuevoIdImagen != idFotoActual) {
          Serial.println("Descargando nueva foto de fondo (Modo No-Sincronizado)...");
          descargarYGuardarImagenFS(nuevaUrlImagen, nuevoIdImagen);
        } else {
          Serial.printf("Foto %d ya en pantalla en modo no-sincro. Ignorando.\n", nuevoIdImagen);
        }
      }
    }
  }

  // 2. PROCESAMIENTO DE BANDERAS DE BORRADO (Fuera del callback)
  if (tengoFotoBorrada) {
    tengoFotoBorrada = false;

    if (idFotoABorrar > 0 && fsInicializada) {
      String rutaBorrando = getRutaFoto(idFotoABorrar);
      if (LittleFS.exists(rutaBorrando)) {
        LittleFS.remove(rutaBorrando);
        Serial.printf("Foto %d borrada fisicamente de la memoria interna.\n",
                      idFotoABorrar);

        // Remover de la lista en memoria
        auto it = std::remove(listaFotoIds.begin(), listaFotoIds.end(),
                              idFotoABorrar);
        listaFotoIds.erase(it, listaFotoIds.end());

        // Si la foto borrada era la que se estaba mostrando, refrescar
        if (modoActual == MODO_SINCRONIZADO ||
            indiceFotoLocal >= listaFotoIds.size()) {
          sincronizarUltimaFoto();
        } else if (!listaFotoIds.empty()) {
          mostrarImagenLocalPorId(listaFotoIds[indiceFotoLocal]);
        }
      }
    }
  }

  // 3. PROCESAR LECTURA TÁCTIL (Touch)
  if (fsInicializada) {
    procesarTactil();
  }

  // 4. LÓGICA DEL MODO CARRUSEL AUTOMÁTICO
  if (modoActual == MODO_CARRUSEL && !listaFotoIds.empty() && !menuAbierto &&
      !menuConfiguracionesAbierto) {
    unsigned long ahora = millis();
    if (ahora - ultimoCambioCarrusel >= intervaloCarrusel) {
      ultimoCambioCarrusel = ahora;
      mostrarFotoSiguienteLocal();
    }
  }

  // Nota: Se ha eliminado el temporizador de notificaciones para el modo
  // Sincronizado. La notificación "Subido por: [Usuario]" permanece en pantalla
  // indefinidamente flotando en su ventana de cristal sin forzar recargas
  // visuales.

  delay(10);
}
