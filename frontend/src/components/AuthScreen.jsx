import React, { useState } from 'react';
import { loginUser, registerUser } from '../api/galeria';

const AuthScreen = ({ onLoginSuccess }) => {
  const [isRegister, setIsRegister] = useState(false);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [email, setEmail] = useState('');
  const [name, setName] = useState('');
  
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setSuccess('');
    setLoading(true);

    if (!username.trim() || !password.trim()) {
      setError('Por favor, ingresa usuario y contraseña.');
      setLoading(false);
      return;
    }

    try {
      if (isRegister) {
        // Flujo de registro
        const res = await registerUser(username, password, email, name);
        setSuccess(res.message || 'Registro exitoso. Espera la aprobación del administrador.');
        // Limpiar campos de registro
        setUsername('');
        setPassword('');
        setEmail('');
        setName('');
      } else {
        // Flujo de login
        const res = await loginUser(username, password);
        localStorage.setItem('token_cuadrito', res.token);
        localStorage.setItem('usuario_cuadrito', res.name);
        onLoginSuccess(res.token, res.name);
      }
    } catch (err) {
      console.error(err);
      if (err.response && err.response.data && err.response.data.error) {
        setError(err.response.data.error);
      } else {
        setError('Ocurrió un error en el servidor. Inténtalo de nuevo.');
      }
    } finally {
      setLoading(false);
    }
  };

  const alternarModo = () => {
    setIsRegister(!isRegister);
    setError('');
    setSuccess('');
    setUsername('');
    setPassword('');
    setEmail('');
    setName('');
  };

  return (
    <div className="auth-escena">
      <div className="auth-card">
        <h1 className="auth-logo">Nuestro Cuadrito</h1>
        <h2 className="auth-subtitulo">
          {isRegister ? 'Crear cuenta de amor' : 'Inicia sesión para ver tu regalo'}
        </h2>

        {error && <div className="auth-alerta error">{error}</div>}
        {success && <div className="auth-alerta success">{success}</div>}

        <form onSubmit={handleSubmit} className="auth-form">
          {isRegister && (
            <>
              <div className="auth-campo">
                <input
                  type="text"
                  placeholder="Tu Nombre Bonito (ej: Mi Amor)"
                  value={name}
                  onChange={(e) => setName(e.target.value)}
                  className="auth-input"
                  maxLength={30}
                  required
                />
              </div>
              <div className="auth-campo">
                <input
                  type="email"
                  placeholder="Correo electrónico (opcional)"
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  className="auth-input"
                />
              </div>
            </>
          )}

          <div className="auth-campo">
            <input
              type="text"
              placeholder="Usuario (letras y números)"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              className="auth-input"
              autoComplete="username"
              required
            />
          </div>

          <div className="auth-campo">
            <input
              type="password"
              placeholder="Contraseña"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              className="auth-input"
              autoComplete="current-password"
              required
            />
          </div>

          <button type="submit" className="btn-accion btn-auth" disabled={loading}>
            {loading ? 'Procesando...' : isRegister ? 'Registrarse' : 'Entrar'}
          </button>
        </form>

        <div className="auth-toggle">
          <p>
            {isRegister ? '¿Ya tienes una cuenta?' : '¿No tienes una cuenta aún?'}
            <button onClick={alternarModo} className="btn-toggle-link">
              {isRegister ? ' Inicia Sesión' : ' Regístrate aquí'}
            </button>
          </p>
        </div>
      </div>
    </div>
  );
};

export default AuthScreen;
