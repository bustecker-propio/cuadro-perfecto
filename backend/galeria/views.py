from rest_framework import generics
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status
from rest_framework.permissions import IsAuthenticated, AllowAny
from rest_framework.authtoken.models import Token
from django.contrib.auth import authenticate
from django.contrib.auth.models import User
from django.shortcuts import get_object_or_404
from django.utils import timezone
from rest_framework.exceptions import PermissionDenied, ValidationError
import secrets

from .models import FotoMarco, Galeria, MarcoDispositivo, SolicitudUnion, generar_token_dispositivo
from .serializers import FotoMarcoSerializer, GaleriaSerializer, SolicitudUnionSerializer
from .mqtt_helper import publicar_mensaje_mqtt

# =========================================================================
# 1. Autenticación y Cuentas de Usuario
# =========================================================================

class LoginView(APIView):
    permission_classes = [AllowAny]

    def post(self, request, *args, **kwargs):
        username = request.data.get('username')
        password = request.data.get('password')

        if not username or not password:
            return Response({"error": "Debe proporcionar usuario y contraseña."}, status=status.HTTP_400_BAD_REQUEST)

        try:
            user_obj = User.objects.get(username=username.lower().strip())
            if not user_obj.is_active:
                return Response({"error": "Tu cuenta está pendiente de aprobación por el administrador."}, status=status.HTTP_403_FORBIDDEN)
        except User.DoesNotExist:
            pass

        user = authenticate(username=username.lower().strip(), password=password)
        if user is not None:
            token, _ = Token.objects.get_or_create(user=user)
            return Response({
                "token": token.key,
                "username": user.username,
                "name": user.first_name or user.username
            }, status=status.HTTP_200_OK)
        else:
            return Response({"error": "Credenciales inválidas."}, status=status.HTTP_400_BAD_REQUEST)

class RegisterView(APIView):
    permission_classes = [AllowAny]

    def post(self, request, *args, **kwargs):
        username = request.data.get('username')
        password = request.data.get('password')
        email = request.data.get('email', '')
        name = request.data.get('name', '')

        if not username or not password:
            return Response({"error": "Debe proporcionar usuario y contraseña."}, status=status.HTTP_400_BAD_REQUEST)

        username_clean = username.lower().strip().replace(" ", "_")
        import re
        username_clean = re.sub(r'[^a-z0-9@\.\+\-_]', '', username_clean)

        if not username_clean:
            return Response({"error": "Nombre de usuario inválido."}, status=status.HTTP_400_BAD_REQUEST)

        if User.objects.filter(username=username_clean).exists():
            return Response({"error": "El nombre de usuario ya está registrado."}, status=status.HTTP_400_BAD_REQUEST)

        user = User.objects.create_user(
            username=username_clean,
            email=email,
            password=password,
            first_name=name,
            is_active=False
        )
        return Response({
            "message": "Registro exitoso. Tu cuenta está en espera de aprobación por el administrador.",
            "username": username_clean
        }, status=status.HTTP_201_CREATED)


# =========================================================================
# 2. Gestión de Fotos en Galerías (Protegido)
# =========================================================================

class FotoMarcoListCreateView(generics.ListCreateAPIView):
    permission_classes = [IsAuthenticated]
    serializer_class = FotoMarcoSerializer

    def get_queryset(self):
        user = self.request.user
        user_galerias = user.galerias.all()
        if not user_galerias.exists():
            return FotoMarco.objects.none()

        galeria_id = self.request.query_params.get('galeria')
        if galeria_id:
            try:
                galeria = user_galerias.get(id=galeria_id)
            except (Galeria.DoesNotExist, ValueError):
                raise PermissionDenied("No tienes acceso a esta galería o el ID es inválido.")
            return FotoMarco.objects.filter(galeria=galeria).order_by('-fecha_visualizacion')
        
        return FotoMarco.objects.filter(galeria=user_galerias.first()).order_by('-fecha_visualizacion')

    def perform_create(self, serializer):
        user = self.request.user
        user_galerias = user.galerias.all()
        if not user_galerias.exists():
            raise PermissionDenied("No perteneces a ninguna galería activa. Ponte en contacto con el administrador.")

        usuario_req = self.request.data.get('usuario')
        if not usuario_req:
            usuario_req = user.first_name or user.username

        galeria_id = self.request.data.get('galeria')
        if galeria_id:
            try:
                target_galeria = user_galerias.get(id=galeria_id)
            except (Galeria.DoesNotExist, ValueError):
                raise PermissionDenied("Galería inválida o sin permisos de acceso.")
        else:
            target_galeria = user_galerias.first()

        foto = serializer.save(
            usuario_creador=user,
            usuario=usuario_req,
            galeria=target_galeria
        )

        mqtt_topic = f"cuadros/galeria/{target_galeria.id}/nueva_foto"
        payload = {
            "id": foto.id,
            "url": foto.imagen.url,
            "usuario": foto.usuario
        }
        publicar_mensaje_mqtt(mqtt_topic, payload, retain=True)

class FotoMarcoDetailView(generics.RetrieveDestroyAPIView):
    permission_classes = [IsAuthenticated]
    serializer_class = FotoMarcoSerializer

    def get_queryset(self):
        return FotoMarco.objects.filter(galeria__usuarios=self.request.user)

    def perform_destroy(self, instance):
        id_borrado = instance.id
        url_borrada = instance.imagen.url
        galeria_id = instance.galeria.id

        instance.delete()

        mqtt_topic = f"cuadros/galeria/{galeria_id}/borrar_foto"
        payload = {
            "accion": "eliminar",
            "id": id_borrado,
            "url": url_borrada
        }
        publicar_mensaje_mqtt(mqtt_topic, payload, retain=False)

class FotoMarcoMostrarView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, pk, *args, **kwargs):
        user = self.request.user
        foto = get_object_or_404(FotoMarco, pk=pk, galeria__usuarios=user)

        usuario_req = request.data.get('usuario')
        if not usuario_req:
            usuario_req = user.first_name or user.username

        foto.fecha_visualizacion = timezone.now()
        foto.usuario_creador = user
        foto.usuario = usuario_req
        foto.save(update_fields=['fecha_visualizacion', 'usuario_creador', 'usuario'])

        mqtt_topic = f"cuadros/galeria/{foto.galeria.id}/nueva_foto"
        payload = {
            "id": foto.id,
            "url": foto.imagen.url,
            "usuario": foto.usuario
        }
        publicar_mensaje_mqtt(mqtt_topic, payload, retain=True)

        return Response({"status": "ok", "message": "Imagen forzada en pantalla."}, status=status.HTTP_200_OK)


# =========================================================================
# 3. Flujo Social de Galerías (Creación, Unión y Abandono)
# =========================================================================

class GaleriaListarView(APIView):
    permission_classes = [IsAuthenticated]

    def get(self, request, *args, **kwargs):
        user = request.user
        galerias = user.galerias.all().order_by('-fecha_creacion')
        serializer = GaleriaSerializer(galerias, many=True, context={'request': request})
        return Response(serializer.data, status=status.HTTP_200_OK)

class GaleriaCrearView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = request.user
        nombre = request.data.get('nombre')

        if not nombre or not nombre.strip():
            return Response({"error": "El nombre de la galería es requerido."}, status=status.HTTP_400_BAD_REQUEST)

        nombre_clean = nombre.strip()
        if Galeria.objects.filter(nombre__iexact=nombre_clean).exists():
            return Response({"error": "Ya existe una galería con este nombre. Elige otro único."}, status=status.HTTP_400_BAD_REQUEST)

        # Crear galería asignando al creador y agregándolo como usuario miembro
        galeria = Galeria.objects.create(nombre=nombre_clean, creador=user)
        galeria.usuarios.add(user)

        serializer = GaleriaSerializer(galeria, context={'request': request})
        return Response(serializer.data, status=status.HTTP_201_CREATED)

class SolicitudUnionCrearView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = request.user
        nombre_galeria = request.data.get('nombre_galeria')

        if not nombre_galeria or not nombre_galeria.strip():
            return Response({"error": "Debe proporcionar el nombre de la galería a la que desea unirse."}, status=status.HTTP_400_BAD_REQUEST)

        # Buscar la galería (case-insensitive)
        galeria = get_object_or_404(Galeria, nombre__iexact=nombre_galeria.strip())

        # Si ya es miembro
        if galeria.usuarios.filter(id=user.id).exists():
            return Response({"error": "Ya eres miembro de esta galería."}, status=status.HTTP_400_BAD_REQUEST)

        # Buscar o crear la solicitud de unión
        solicitud, created = SolicitudUnion.objects.get_or_create(
            usuario=user,
            galeria=galeria,
            defaults={'estado': 'pendiente'}
        )

        if not created:
            if solicitud.estado == 'pendiente':
                return Response({"error": "Ya tienes una solicitud de unión pendiente para esta galería."}, status=status.HTTP_400_BAD_REQUEST)
            elif solicitud.estado == 'rechazada':
                # Permitir re-enviar la solicitud si fue rechazada anteriormente
                solicitud.estado = 'pendiente'
                solicitud.save(update_fields=['estado'])

        return Response({"message": f"Solicitud enviada al creador de la galería '{galeria.nombre}'."}, status=status.HTTP_201_CREATED)

class SolicitudUnionPendientesView(APIView):
    permission_classes = [IsAuthenticated]

    def get(self, request, *args, **kwargs):
        user = request.user
        # Obtener las solicitudes de unión dirigidas a galerías de las cuales el usuario es creador/dueño
        solicitudes = SolicitudUnion.objects.filter(galeria__creador=user, estado='pendiente').order_by('-fecha_creacion')
        serializer = SolicitudUnionSerializer(solicitudes, many=True)
        return Response(serializer.data, status=status.HTTP_200_OK)

class SolicitudUnionResponderView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, pk, *args, **kwargs):
        user = request.user
        accion = request.data.get('accion') # 'aprobar' o 'rechazar'

        if accion not in ['aprobar', 'rechazar']:
            return Response({"error": "Acción inválida. Debe ser 'aprobar' o 'rechazar'."}, status=status.HTTP_400_BAD_REQUEST)

        solicitud = get_object_or_404(SolicitudUnion, pk=pk)

        # Verificar que el usuario actual es el creador de la galería destino
        if solicitud.galeria.creador != user:
            raise PermissionDenied("No tienes permisos para responder solicitudes de esta galería.")

        if accion == 'aprobar':
            solicitud.estado = 'aprobada'
            solicitud.galeria.usuarios.add(solicitud.usuario)
        else:
            solicitud.estado = 'rechazada'

        solicitud.save(update_fields=['estado'])
        return Response({"message": f"Solicitud {accion}da con éxito."}, status=status.HTTP_200_OK)

class GaleriaAbandonarView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = request.user
        galeria_id = request.data.get('galeria_id')

        if not galeria_id:
            return Response({"error": "Debe proporcionar el ID de la galería."}, status=status.HTTP_400_BAD_REQUEST)

        galeria = get_object_or_404(Galeria, id=galeria_id, usuarios=user)

        # El dueño no puede abandonar la galería (debe borrarla)
        if galeria.creador == user:
            return Response({"error": "Como creador/dueño, no puedes abandonar la galería. Debes eliminarla si no la necesitas."}, status=status.HTTP_400_BAD_REQUEST)

        galeria.usuarios.remove(user)
        # Limpiar solicitudes previas
        SolicitudUnion.objects.filter(usuario=user, galeria=galeria).delete()

        return Response({"message": f"Has abandonado la galería '{galeria.nombre}' con éxito."}, status=status.HTTP_200_OK)

class GaleriaEliminarView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = request.user
        galeria_id = request.data.get('galeria_id')

        if not galeria_id:
            return Response({"error": "Debe proporcionar el ID de la galería a eliminar."}, status=status.HTTP_400_BAD_REQUEST)

        # Buscar la galería asegurando que el creador sea el usuario actual
        galeria = get_object_or_404(Galeria, id=galeria_id)
        if galeria.creador != user:
            return Response({"error": "Solo el dueño/creador puede eliminar esta galería."}, status=status.HTTP_403_FORBIDDEN)

        nombre_gal = galeria.nombre
        galeria.delete() # Cascada borra fotos, relaciones y solicitudes asociadas
        return Response({"message": f"Galería '{nombre_gal}' eliminada con éxito."}, status=status.HTTP_200_OK)

# =========================================================================
# 4. Sincronización e Integración de Marcos Físicos (Smart Pairing)
# =========================================================================

class MarcoSincronizarView(APIView):
    permission_classes = [AllowAny]

    def get(self, request, *args, **kwargs):
        device_token = request.headers.get('X-Device-Token')
        if not device_token:
            return Response({"error": "Cabecera X-Device-Token no proporcionada."}, status=status.HTTP_401_UNAUTHORIZED)

        marco = get_object_or_404(MarcoDispositivo, token_acceso=device_token)
        galeria = marco.galeria_activa

        if not galeria:
            return Response({
                "dispositivo_id": str(marco.id),
                "galeria_id": None,
                "galeria_nombre": "Ninguna",
                "mqtt_topic": "",
                "mqtt_topic_dispositivo": f"cuadros/dispositivo/{marco.id}/comando",
                "fotos": []
            }, status=status.HTTP_200_OK)

        fotos_query = galeria.fotos.all().order_by('-fecha_visualizacion')
        fotos_serializadas = []
        for f in fotos_query:
            fotos_serializadas.append({
                "id": f.id,
                "imagen": f.imagen.url,
                "usuario": f.usuario or (f.usuario_creador.first_name if f.usuario_creador else "Alguien")
            })

        return Response({
            "dispositivo_id": str(marco.id),
            "galeria_id": str(galeria.id),
            "galeria_nombre": galeria.nombre,
            "mqtt_topic": f"cuadros/galeria/{galeria.id}/nueva_foto",
            "mqtt_topic_dispositivo": f"cuadros/dispositivo/{marco.id}/comando",
            "fotos": fotos_serializadas
        }, status=status.HTTP_200_OK)

class MarcoCambiarGaleriaView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = self.request.user
        marco_id = request.data.get('marco_id')
        galeria_id = request.data.get('galeria_id')

        if not galeria_id:
            return Response({"error": "Debe proporcionar galeria_id."}, status=status.HTTP_400_BAD_REQUEST)

        try:
            target_galeria = user.galerias.get(id=galeria_id)
        except (Galeria.DoesNotExist, ValueError):
            return Response({"error": "Galería no encontrada o sin permisos de acceso."}, status=status.HTTP_403_FORBIDDEN)

        # Si se especifica un marco_id, cambiamos solo para ese
        if marco_id:
            marco = get_object_or_404(
                MarcoDispositivo, 
                id=marco_id, 
                creador=user
            )
            marcos = [marco]
        else:
            # Si no se especifica, cambiar para todos los marcos que pertenecen al usuario
            marcos = MarcoDispositivo.objects.filter(creador=user)

        for m in marcos:
            m.galeria_activa = target_galeria
            m.save(update_fields=['galeria_activa'])

            dispositivo_topic = f"cuadros/dispositivo/{m.id}/comando"
            comando_payload = {
                "accion": "cambiar_galeria",
                "galeria_id": str(target_galeria.id),
                "nombre": target_galeria.nombre
            }
            publicar_mensaje_mqtt(dispositivo_topic, comando_payload, retain=True)

        return Response({
            "status": "ok",
            "message": f"Galería cambiada a '{target_galeria.nombre}' con éxito en {len(marcos)} dispositivo(s).",
            "galeria_id": str(target_galeria.id)
        }, status=status.HTTP_200_OK)

# --- ENDPOINTS DE EMPAREJAMIENTO DE DISPOSITIVOS (PIN DE 6 DÍGITOS) ---

class MarcoVincularCodigoView(APIView):
    permission_classes = [AllowAny]

    def post(self, request, *args, **kwargs):
        mac = request.data.get('mac')
        if not mac:
            return Response({"error": "La dirección MAC es requerida."}, status=status.HTTP_400_BAD_REQUEST)

        # Normalizar MAC (ej: "a1:b2:c3:d4:e5:f6" -> "A1:B2:C3:D4:E5:F6")
        mac_clean = mac.strip().upper()

        # Buscar o crear el dispositivo por su dirección MAC física
        marco, created = MarcoDispositivo.objects.get_or_create(
            mac_address=mac_clean,
            defaults={
                'nombre': f"Cuadrito {mac_clean[-8:].replace(':', '')}",
                'token_acceso': generar_token_dispositivo()
            }
        )

        # Generar un código PIN de 6 dígitos aleatorio
        pin = str(secrets.randbelow(900000) + 100000)
        marco.codigo_vinculacion = pin
        # El PIN expira en 10 minutos
        marco.codigo_expiracion = timezone.now() + timezone.timedelta(minutes=10)
        marco.save(update_fields=['codigo_vinculacion', 'codigo_expiracion'])

        return Response({
            "codigo": pin,
            "expira_en_segundos": 600
        }, status=status.HTTP_200_OK)

class MarcoVincularEstadoView(APIView):
    permission_classes = [AllowAny]

    def get(self, request, *args, **kwargs):
        mac = request.query_params.get('mac')
        if not mac:
            return Response({"error": "La dirección MAC es requerida."}, status=status.HTTP_400_BAD_REQUEST)

        mac_clean = mac.strip().upper()
        marco = get_object_or_404(MarcoDispositivo, mac_address=mac_clean)

        # Si el cuadro ya está vinculado a alguna galería activa, retornamos su token
        if marco.galeria_activa is not None:
            return Response({
                "estado": "vinculado",
                "token": marco.token_acceso,
                "galeria_nombre": marco.galeria_activa.nombre
            }, status=status.HTTP_200_OK)
        else:
            return Response({
                "estado": "esperando"
            }, status=status.HTTP_200_OK)

class MarcoVincularConfirmarView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request, *args, **kwargs):
        user = request.user
        codigo = request.data.get('codigo')
        galeria_id = request.data.get('galeria_id')

        if not codigo:
            return Response({"error": "Debe introducir el código de 6 dígitos expuesto en el cuadro."}, status=status.HTTP_400_BAD_REQUEST)

        # Buscar el marco que tenga el código activo y no expirado
        try:
            marco = MarcoDispositivo.objects.get(
                codigo_vinculacion=codigo.strip(),
                codigo_expiracion__gt=timezone.now()
            )
        except MarcoDispositivo.DoesNotExist:
            return Response({"error": "Código PIN inválido, expirado o ya utilizado. Genera uno nuevo en el cuadro."}, status=status.HTTP_400_BAD_REQUEST)

        # Resolver la galería a la cual se vinculará el cuadro
        user_galerias = user.galerias.all()
        if not user_galerias.exists():
            # Crear una galería por defecto automáticamente si el usuario no tiene ninguna
            nombre_por_defecto = f"Galería de {user.first_name or user.username}"
            base_nombre = nombre_por_defecto
            contador = 1
            while Galeria.objects.filter(nombre__iexact=nombre_por_defecto).exists():
                nombre_por_defecto = f"{base_nombre} {contador}"
                contador += 1
            
            target_galeria = Galeria.objects.create(nombre=nombre_por_defecto, creador=user)
            target_galeria.usuarios.add(user)
        else:
            if galeria_id:
                try:
                    target_galeria = user_galerias.get(id=galeria_id)
                except (Galeria.DoesNotExist, ValueError):
                    return Response({"error": "La galería seleccionada es inválida o no tienes permisos."}, status=status.HTTP_403_FORBIDDEN)
            else:
                # Default al primero
                target_galeria = user_galerias.first()

        # Vincular el cuadro
        marco.creador = user
        marco.galerias_permitidas.add(target_galeria)
        marco.galeria_activa = target_galeria
        
        # Limpiar el código PIN temporal usado
        marco.codigo_vinculacion = None
        marco.codigo_expiracion = None
        marco.save()

        return Response({
            "status": "ok",
            "message": f"¡Cuadro '{marco.nombre}' vinculado con éxito a la galería '{target_galeria.nombre}'!",
            "dispositivo_id": str(marco.id),
            "galeria_nombre": target_galeria.nombre
        }, status=status.HTTP_200_OK)

class MarcoListarView(APIView):
    permission_classes = [IsAuthenticated]
    
    def get(self, request, *args, **kwargs):
        user = request.user
        marcos = MarcoDispositivo.objects.filter(creador=user)
        marcos_data = []
        for m in marcos:
            marcos_data.append({
                "id": str(m.id),
                "nombre": m.nombre,
                "galeria_activa_nombre": m.galeria_activa.nombre if m.galeria_activa else "Ninguna"
            })
        return Response(marcos_data, status=status.HTTP_200_OK)