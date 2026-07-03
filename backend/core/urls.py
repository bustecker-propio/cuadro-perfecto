from django.contrib import admin
from django.urls import path, include

urlpatterns = [
    path('admin/', admin.site.urls),
    path('api/galeria/', include('galeria.urls')), # Conecta tu app aquí
]