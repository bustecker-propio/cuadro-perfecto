# Contexto del Proyecto: Nuestro Cuadrito (Regalo de Aniversario)

## 1. Objetivo del Proyecto
Sistema IoT full-stack para un marco de fotos digital dedicado. Consta de una interfaz web para subir/gestionar fotos y un microcontrolador que muestra las imágenes, con un diseño web altamente personalizado y romántico.

## 2. Arquitectura del Sistema
- **Backend**: Django con Django Rest Framework (DRF) para la API REST. Expuesto en la red local.
- **Frontend**: React (Vite) utilizando JavaScript puro.
- **Almacenamiento**: Amazon S3 (bucket configurado con CORS para permitir carga/lectura directa desde el frontend en React).
- **IoT / Hardware**: Microcontrolador ESP32 (código a desarrollar en C++ con PlatformIO) conectado vía protocolo MQTT.

## 3. Estado Actual del Frontend (Ya implementado)
- **Splash Screen**: Pantalla de bienvenida interactiva con animaciones CSS (corazones SVG pulsando asíncronamente) y transición de texto "Feliz aniversario", controlada por estados de React.
- **UI/UX Principal**: Diseño de panel estilo "Glassmorphism" (cristal esmerilado).
- **Visualización**: Carrusel de fotos en entorno 3D CSS (efecto apilado/coverflow), renderizando las imágenes provenientes del bucket de AWS mediante Axios.
- **Interactividad**: Lógica completa para navegar (`irFotoAnterior`, `irFotoSiguiente`), añadir (con `FormData`) y eliminar imágenes, actualizando la vista dinámicamente.
- **Responsive**: Media queries implementadas para que la interfaz se use cómodamente desde un celular.

## 4. Próximo Hito (Fase de Hardware)
- Iniciar proyecto en PlatformIO.
- Programar el ESP32 para conectarse al WiFi, suscribirse al broker MQTT, escuchar los eventos de cambio de foto de Django y renderizar la imagen en la pantalla física.
