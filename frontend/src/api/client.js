import axios from 'axios';

const API_BASE_URL = import.meta.env.VITE_API_BASE;

const apiClient = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Interceptor para inyectar automáticamente el Token de autenticación en cada petición
apiClient.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('token_cuadrito');
    if (token) {
      config.headers.Authorization = `Token ${token}`;
    }
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

export default apiClient;