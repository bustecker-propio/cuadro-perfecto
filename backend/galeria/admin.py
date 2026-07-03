from django.contrib import admin
from .models import Galeria, FotoMarco, MarcoDispositivo, SolicitudUnion

@admin.register(Galeria)
class GaleriaAdmin(admin.ModelAdmin):
    list_display = ('id', 'nombre', 'creador', 'fecha_creacion')
    search_fields = ('nombre',)

@admin.register(FotoMarco)
class FotoMarcoAdmin(admin.ModelAdmin):
    list_display = ('id', 'galeria', 'titulo', 'usuario', 'fecha_subida')
    list_filter = ('galeria', 'usuario')

@admin.register(MarcoDispositivo)
class MarcoDispositivoAdmin(admin.ModelAdmin):
    list_display = ('id', 'nombre', 'creador', 'mac_address', 'galeria_activa', 'fecha_registro')
    search_fields = ('nombre', 'mac_address')

@admin.register(SolicitudUnion)
class SolicitudUnionAdmin(admin.ModelAdmin):
    list_display = ('id', 'usuario', 'galeria', 'estado', 'fecha_creacion')
    list_filter = ('estado', 'galeria')
