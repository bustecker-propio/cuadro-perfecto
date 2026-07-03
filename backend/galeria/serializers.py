from rest_framework import serializers
from .models import FotoMarco, Galeria, SolicitudUnion
from django.contrib.auth.models import User

class FotoMarcoSerializer(serializers.ModelSerializer):
    usuario_creador_nombre = serializers.ReadOnlyField(source='usuario_creador.first_name')
    usuario_creador_username = serializers.ReadOnlyField(source='usuario_creador.username')

    class Meta:
        model = FotoMarco
        fields = [
            'id', 
            'titulo', 
            'imagen', 
            'fecha_subida', 
            'fecha_visualizacion',
            'usuario', # String antiguo de compatibilidad
            'usuario_creador_nombre',
            'usuario_creador_username',
            'galeria'
        ]
        read_only_fields = ['fecha_subida', 'fecha_visualizacion']

class GaleriaSerializer(serializers.ModelSerializer):
    creador_username = serializers.ReadOnlyField(source='creador.username')
    es_dueno = serializers.SerializerMethodField()

    class Meta:
        model = Galeria
        fields = ['id', 'nombre', 'creador', 'creador_username', 'es_dueno', 'fecha_creacion']
        read_only_fields = ['id', 'creador', 'fecha_creacion']

    def get_es_dueno(self, obj):
        request = self.context.get('request')
        if request and request.user.is_authenticated:
            return obj.creador == request.user
        return False

class SolicitudUnionSerializer(serializers.ModelSerializer):
    usuario_username = serializers.ReadOnlyField(source='usuario.username')
    usuario_nombre = serializers.ReadOnlyField(source='usuario.first_name')
    galeria_nombre = serializers.ReadOnlyField(source='galeria.nombre')

    class Meta:
        model = SolicitudUnion
        fields = ['id', 'usuario', 'usuario_username', 'usuario_nombre', 'galeria', 'galeria_nombre', 'estado', 'fecha_creacion']
        read_only_fields = ['id', 'usuario', 'galeria', 'estado', 'fecha_creacion']