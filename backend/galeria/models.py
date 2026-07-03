import os
import uuid
import secrets
from django.db import models 
from django.utils import timezone
from django.contrib.auth.models import User
from PIL import Image, ImageOps
from io import BytesIO
from django.core.files.base import ContentFile

def ruta_imagen_unica(instance, filename):
    # Genera un nombre único aleatorio usando UUID y fuerza la extensión .jpg
    nombre_unico = f"{uuid.uuid4()}.jpg"
    return os.path.join('fotos_marco/', nombre_unico)

class Galeria(models.Model):
    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    nombre = models.CharField(max_length=100, unique=True, verbose_name="Nombre de la Galería")
    creador = models.ForeignKey(User, on_delete=models.CASCADE, related_name="galerias_creadas", null=True, blank=True, verbose_name="Dueño/Creador")
    usuarios = models.ManyToManyField(User, related_name="galerias", verbose_name="Usuarios autorizados")
    fecha_creacion = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return self.nombre

class FotoMarco(models.Model):
    galeria = models.ForeignKey(Galeria, on_delete=models.CASCADE, related_name="fotos", null=True, blank=True, verbose_name="Galería")
    titulo = models.CharField(max_length=100, blank=True, null=True, verbose_name="Título opcional")
    imagen = models.ImageField(upload_to=ruta_imagen_unica, verbose_name="Archivo de imagen")
    fecha_subida = models.DateTimeField(auto_now_add=True, verbose_name="Fecha de subida")
    fecha_visualizacion = models.DateTimeField(default=timezone.now, verbose_name="Última vez mostrada")
    usuario_creador = models.ForeignKey(User, on_delete=models.SET_NULL, null=True, blank=True, related_name="fotos_subidas", verbose_name="Usuario creador")
    usuario = models.CharField(max_length=100, blank=True, null=True, default="Alguien", verbose_name="Nombre de usuario (antiguo)")

    def __str__(self):
        return self.titulo if self.titulo else f"Foto {self.id} - {self.fecha_subida.strftime('%Y-%m-%d %H:%M')}"

    def save(self, *args, **kwargs):
        """
        Sobrescribimos el método save para procesar la imagen automáticamente
        antes de que se guarde en el disco o en el almacenamiento en la nube.
        """
        if self.imagen:
            img = Image.open(self.imagen)
            ancho_objetivo = 480
            alto_objetivo = 320
            img_optimizada = ImageOps.fit(img, (ancho_objetivo, alto_objetivo), Image.Resampling.LANCZOS)
            buffer_ram = BytesIO()
            img_optimizada.convert('RGB').save(buffer_ram, format='JPEG', quality=85)
            buffer_ram.seek(0)
            nombre_base = os.path.splitext(self.imagen.name)[0]
            nuevo_nombre = f"{nombre_base}.jpg"
            self.imagen.save(nuevo_nombre, ContentFile(buffer_ram.read()), save=False)
            
        super().save(*args, **kwargs)
    
    def delete(self, *args, **kwargs):
        if self.imagen:
            self.imagen.delete(save=False)
        super().delete(*args, **kwargs)

def generar_token_dispositivo():
    return secrets.token_hex(32)

class MarcoDispositivo(models.Model):
    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    nombre = models.CharField(max_length=100, verbose_name="Nombre del marco")
    creador = models.ForeignKey(User, on_delete=models.SET_NULL, null=True, blank=True, related_name="marcos_propios", verbose_name="Usuario dueño/creador")
    galerias_permitidas = models.ManyToManyField(Galeria, related_name="marcos_autorizados", verbose_name="Galerías permitidas")
    galeria_activa = models.ForeignKey(Galeria, on_delete=models.SET_NULL, null=True, blank=True, related_name="marcos_activos", verbose_name="Galería activa")
    token_acceso = models.CharField(max_length=64, default=generar_token_dispositivo, unique=True, verbose_name="Token de acceso")
    
    # Nuevos campos para emparejamiento inteligente (Smart Pairing)
    mac_address = models.CharField(max_length=17, unique=True, null=True, blank=True, verbose_name="Dirección MAC")
    codigo_vinculacion = models.CharField(max_length=6, null=True, blank=True, verbose_name="Código PIN de vinculación")
    codigo_expiracion = models.DateTimeField(null=True, blank=True, verbose_name="Fecha de expiración del código PIN")
    
    fecha_registro = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"{self.nombre} (Activa: {self.galeria_activa.nombre if self.galeria_activa else 'Ninguna'})"

class SolicitudUnion(models.Model):
    ESTADO_CHOICES = (
        ('pendiente', 'Pendiente'),
        ('aprobada', 'Aprobada'),
        ('rechazada', 'Rechazada'),
    )
    usuario = models.ForeignKey(User, on_delete=models.CASCADE, related_name="solicitudes_enviadas")
    galeria = models.ForeignKey(Galeria, on_delete=models.CASCADE, related_name="solicitudes_recibidas")
    estado = models.CharField(max_length=20, choices=ESTADO_CHOICES, default='pendiente')
    fecha_creacion = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ('usuario', 'galeria')

    def __str__(self):
        return f"{self.usuario.username} -> {self.galeria.nombre} ({self.estado})"