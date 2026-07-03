from django.urls import path
from .views import (
    FotoMarcoListCreateView, 
    FotoMarcoDetailView, 
    FotoMarcoMostrarView,
    LoginView,
    RegisterView,
    MarcoSincronizarView,
    MarcoCambiarGaleriaView,
    GaleriaListarView,
    GaleriaCrearView,
    GaleriaAbandonarView,
    GaleriaEliminarView,
    SolicitudUnionCrearView,
    SolicitudUnionPendientesView,
    SolicitudUnionResponderView,
    MarcoVincularCodigoView,
    MarcoVincularEstadoView,
    MarcoVincularConfirmarView,
    MarcoListarView,
    MarcoDesvincularView
)

urlpatterns = [
    # Autenticación
    path('login/', LoginView.as_view(), name='auth_login'),
    path('register/', RegisterView.as_view(), name='auth_register'),
    
    # Gestión de Fotos
    path('fotos/', FotoMarcoListCreateView.as_view(), name='lista_crear_fotos'),
    path('fotos/<int:pk>/', FotoMarcoDetailView.as_view(), name='detalle_eliminar_foto'),
    path('fotos/<int:pk>/mostrar/', FotoMarcoMostrarView.as_view(), name='mostrar_foto_en_cuadros'),
    
    # Gestión de Galerías
    path('galerias/', GaleriaListarView.as_view(), name='galeria_listar'),
    path('galerias/crear/', GaleriaCrearView.as_view(), name='galeria_crear'),
    path('galerias/abandonar/', GaleriaAbandonarView.as_view(), name='galeria_abandonar'),
    path('galerias/eliminar/', GaleriaEliminarView.as_view(), name='galeria_eliminar'),
    path('galerias/solicitudes/unirse/', SolicitudUnionCrearView.as_view(), name='solicitud_union_crear'),
    path('galerias/solicitudes/pendientes/', SolicitudUnionPendientesView.as_view(), name='solicitud_union_pendientes'),
    path('galerias/solicitudes/<int:pk>/responder/', SolicitudUnionResponderView.as_view(), name='solicitud_union_responder'),
    
    # Sincronización y Vinculación de Marcos (ESP32 / Web)
    path('marco/sincronizar/', MarcoSincronizarView.as_view(), name='marco_sincronizar'),
    path('marco/cambiar-galeria/', MarcoCambiarGaleriaView.as_view(), name='marco_cambiar_galeria'),
    path('marco/vincular/codigo/', MarcoVincularCodigoView.as_view(), name='marco_vincular_codigo'),
    path('marco/vincular/estado/', MarcoVincularEstadoView.as_view(), name='marco_vincular_estado'),
    path('marco/vincular/confirmar/', MarcoVincularConfirmarView.as_view(), name='marco_vincular_confirmar'),
    path('marco/mis-marcos/', MarcoListarView.as_view(), name='marco_listar_usuario'),
    path('marco/<uuid:pk>/desvincular/', MarcoDesvincularView.as_view(), name='marco_desvincular'),
]