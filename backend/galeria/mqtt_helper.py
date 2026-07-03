import paho.mqtt.publish as publish
from django.conf import settings
import json
import ssl

def publicar_mensaje_mqtt(topic, payload, retain=True):
    # Cargar configuraciones de settings o usar valores por defecto
    mqtt_server = getattr(settings, 'MQTT_SERVER', 'broker.hivemq.com')
    mqtt_port = getattr(settings, 'MQTT_PORT', 1883)
    mqtt_user = getattr(settings, 'MQTT_USER', None)
    mqtt_pass = getattr(settings, 'MQTT_PASS', None)
    mqtt_use_tls = getattr(settings, 'MQTT_USE_TLS', False)

    auth = None
    if mqtt_user and mqtt_pass:
        auth = {'username': mqtt_user, 'password': mqtt_pass}

    tls = None
    if mqtt_use_tls:
        # Configurar TLS/SSL para conexiones seguras (ej. HiveMQ Cloud puerto 8883)
        context = ssl.create_default_context()
        tls = {
            'ca_certs': None,
            'certfile': None,
            'keyfile': None,
            'cert_reqs': ssl.CERT_REQUIRED,
            'tls_version': ssl.PROTOCOL_TLSv1_2,
            'ciphers': None
        }

    try:
        publish.single(
            topic=topic,
            payload=json.dumps(payload),
            hostname=mqtt_server,
            port=mqtt_port,
            auth=auth,
            tls=tls,
            retain=retain
        )
        print(f"[MQTT] Mensaje publicado en [{topic}] (retain={retain})")
    except Exception as e:
        print(f"[MQTT] Error al publicar en [{topic}]: {e}")
