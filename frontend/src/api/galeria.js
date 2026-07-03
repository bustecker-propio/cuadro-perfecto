import apiClient from './client';

export const getFotos = async (galeria_id = null) => {
  const url = galeria_id ? `/galeria/fotos/?galeria=${galeria_id}` : '/galeria/fotos/';
  const response = await apiClient.get(url);
  return response.data;
};

export const subirFoto = async (formData) => {
  // Para enviar archivos (imágenes), necesitamos usar FormData y cambiar el header
  const response = await apiClient.post('/galeria/fotos/', formData, {
    headers: {
      'Content-Type': 'multipart/form-data',
    },
  });
  return response.data;
};

export const eliminarFoto = async (id) => {
  const response = await apiClient.delete(`/galeria/fotos/${id}/`);
  return response.data;
};

export const mostrarFotoEnCuadros = async (id, usuario) => {
  const response = await apiClient.post(`/galeria/fotos/${id}/mostrar/`, { usuario });
  return response.data;
};

export const loginUser = async (username, password) => {
  const response = await apiClient.post('/galeria/login/', { username, password });
  return response.data;
};

export const registerUser = async (username, password, email, name) => {
  const response = await apiClient.post('/galeria/register/', { username, password, email, name });
  return response.data;
};

export const getGalerias = async () => {
  const response = await apiClient.get('/galeria/galerias/');
  return response.data;
};

export const crearGaleria = async (nombre) => {
  const response = await apiClient.post('/galeria/galerias/crear/', { nombre });
  return response.data;
};

export const abandonarGaleria = async (galeria_id) => {
  const response = await apiClient.post('/galeria/galerias/abandonar/', { galeria_id });
  return response.data;
};

export const solicitarUnirse = async (nombre_galeria) => {
  const response = await apiClient.post('/galeria/galerias/solicitudes/unirse/', { nombre_galeria });
  return response.data;
};

export const getSolicitudesPendientes = async () => {
  const response = await apiClient.get('/galeria/galerias/solicitudes/pendientes/');
  return response.data;
};

export const responderSolicitud = async (id, accion) => {
  const response = await apiClient.post(`/galeria/galerias/solicitudes/${id}/responder/`, { accion });
  return response.data;
};

export const vincularCuadro = async (codigo, galeria_id) => {
  // galeria_id puede ser opcional/nulo ahora
  const response = await apiClient.post('/galeria/marco/vincular/confirmar/', { codigo, galeria_id });
  return response.data;
};

export const getMisMarcos = async () => {
  const response = await apiClient.get('/galeria/marco/mis-marcos/');
  return response.data;
};

export const eliminarGaleria = async (galeria_id) => {
  const response = await apiClient.post('/galeria/galerias/eliminar/', { galeria_id });
  return response.data;
};

export const seleccionarGaleriaActiva = async (galeria_id) => {
  const response = await apiClient.post('/galeria/marco/cambiar-galeria/', { galeria_id });
  return response.data;
};
export const desvincularCuadro = async (marcoId) => {
  const response = await apiClient.delete(/galeria/marco//desvincular/);
  return response.data;
};

