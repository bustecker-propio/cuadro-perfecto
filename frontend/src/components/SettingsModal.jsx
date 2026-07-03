import React, { useState, useEffect, useRef } from 'react';
import { 
  getGalerias, 
  crearGaleria, 
  abandonarGaleria, 
  eliminarGaleria, 
  solicitarUnirse, 
  getSolicitudesPendientes, 
  responderSolicitud, 
  vincularCuadro 
} from '../api/galeria';

const SettingsModal = ({ onClose, activeGaleriaId, onGaleriaChange, resaltarVincular, onVincularSuccess }) => {
  const [galerias, setGalerias] = useState([]);
  const [solicitudes, setSolicitudes] = useState([]);
  
  // Estados para formularios
  const [nuevaGaleria, setNuevaGaleria] = useState('');
  const [nombreBuscar, setNombreBuscar] = useState('');
  const [codigoPin, setCodigoPin] = useState('');
  
  // Mensajes y cargas
  const [mensajeGaleria, setMensajeGaleria] = useState({ texto: '', tipo: '' });
  const [mensajeBuscar, setMensajeBuscar] = useState({ texto: '', tipo: '' });
  const [mensajePin, setMensajePin] = useState({ texto: '', tipo: '' });
  
  const [cargando, setCargando] = useState(false);
  const vincularRef = useRef(null);

  useEffect(() => {
    cargarDatos();
  }, []);

  useEffect(() => {
    if (resaltarVincular && vincularRef.current) {
      const timer = setTimeout(() => {
        vincularRef.current.scrollIntoView({ behavior: 'smooth', block: 'center' });
      }, 350);
      return () => clearTimeout(timer);
    }
  }, [resaltarVincular]);

  const cargarDatos = async () => {
    try {
      const listaGals = await getGalerias();
      setGalerias(listaGals);
      
      const listaSols = await getSolicitudesPendientes();
      setSolicitudes(listaSols);
    } catch (error) {
      console.error("Error al cargar ajustes:", error);
    }
  };

  const handleCrearGaleria = async (e) => {
    e.preventDefault();
    if (!nuevaGaleria.trim()) return;
    setMensajeGaleria({ texto: '', tipo: '' });
    setCargando(true);
    try {
      const res = await crearGaleria(nuevaGaleria);
      setMensajeGaleria({ texto: `Galería "${res.nombre}" creada con éxito.`, tipo: 'success' });
      setNuevaGaleria('');
      await cargarDatos();
      if (onGaleriaChange) onGaleriaChange(res.id);
    } catch (err) {
      setMensajeGaleria({ 
        texto: err.response?.data?.error || 'Error al crear la galería.', 
        tipo: 'error' 
      });
    } finally {
      setCargando(false);
    }
  };

  const handleSolicitarUnirse = async (e) => {
    e.preventDefault();
    if (!nombreBuscar.trim()) return;
    setMensajeBuscar({ texto: '', tipo: '' });
    setCargando(true);
    try {
      const res = await solicitarUnirse(nombreBuscar);
      setMensajeBuscar({ texto: res.message || 'Petición enviada.', tipo: 'success' });
      setNombreBuscar('');
    } catch (err) {
      setMensajeBuscar({ 
        texto: err.response?.data?.error || 'No se encontró la galería o ocurrió un error.', 
        tipo: 'error' 
      });
    } finally {
      setCargando(false);
    }
  };

  const handleVincularCuadro = async (e) => {
    e.preventDefault();
    if (!codigoPin.trim()) return;
    setMensajePin({ texto: '', tipo: '' });
    setCargando(true);
    
    // Vincular al ID de la galería activa si existe
    const targetGalId = activeGaleriaId || (galerias.length > 0 ? galerias[0].id : null);

    try {
      const res = await vincularCuadro(codigoPin, targetGalId);
      setMensajePin({ texto: res.message || '¡Cuadro vinculado con éxito!', tipo: 'success' });
      setCodigoPin('');
      // Cargar galerías y datos de nuevo (para mostrar la galería por defecto creada)
      await cargarDatos();
      if (onVincularSuccess) onVincularSuccess();
    } catch (err) {
      setMensajePin({ 
        texto: err.response?.data?.error || 'Código PIN incorrecto o expirado.', 
        tipo: 'error' 
      });
    } finally {
      setCargando(false);
    }
  };

  const handleResponderSolicitud = async (id, accion) => {
    try {
      await responderSolicitud(id, accion);
      await cargarDatos();
    } catch (error) {
      console.error("Error al responder solicitud:", error);
    }
  };

  const handleAbandonarGaleria = async (id, nombre) => {
    if (!window.confirm(`¿Estás seguro de que deseas abandonar la galería "${nombre}"?`)) return;
    try {
      await abandonarGaleria(id);
      await cargarDatos();
      if (activeGaleriaId === id && galerias.length > 1) {
        const restante = galerias.find(g => g.id !== id);
        if (onGaleriaChange && restante) onGaleriaChange(restante.id);
      }
    } catch (error) {
      console.error("Error al abandonar galería:", error);
    }
  };

  const handleEliminarGaleria = async (id, nombre) => {
    if (!window.confirm(`¿Estás seguro de que deseas eliminar permanentemente la galería "${nombre}"?\nEsta acción es irreversible y borrará todas las fotos asociadas.`)) return;
    try {
      await eliminarGaleria(id);
      await cargarDatos();
      if (activeGaleriaId === id) {
        const restantes = galerias.filter(g => g.id !== id);
        if (onGaleriaChange) {
          onGaleriaChange(restantes.length > 0 ? restantes[0].id : null);
        }
      }
    } catch (error) {
      console.error("Error al eliminar galería:", error);
      alert(error.response?.data?.error || "Error al eliminar la galería.");
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-card">
        <button onClick={onClose} className="btn-close-modal">✕</button>
        
        <h2 className="modal-title">Configuración de Galerías</h2>
        
        {/* SECCIÓN 1: SELECCIONAR GALERÍA ACTIVA */}
        <div className="modal-seccion">
          <h3>Mis Galerías</h3>
          <p className="modal-explicacion">Haz clic para cambiar la galería activa en tu pantalla web.</p>
          <div className="galerias-list">
            {galerias.map(g => (
              <div 
                key={g.id} 
                className={`galeria-item ${g.id === activeGaleriaId ? 'activa' : ''}`}
                onClick={() => onGaleriaChange(g.id)}
              >
                <div className="galeria-info">
                  <span className="galeria-badge">❤</span>
                  <span className="galeria-name">{g.nombre}</span>
                  <span className="galeria-owner">
                    ({g.es_dueno ? 'Tú eres el dueño' : `Dueño: ${g.creador_username}`})
                  </span>
                </div>
                {g.es_dueno ? (
                  <button 
                    onClick={(e) => {
                      e.stopPropagation();
                      handleEliminarGaleria(g.id, g.nombre);
                    }} 
                    className="btn-delete-galeria"
                  >
                    Eliminar
                  </button>
                ) : (
                  <button 
                    onClick={(e) => {
                      e.stopPropagation();
                      handleAbandonarGaleria(g.id, g.nombre);
                    }} 
                    className="btn-leave-galeria"
                  >
                    Salir
                  </button>
                )}
              </div>
            ))}
          </div>
        </div>

        {/* SECCIÓN 2: CREAR NUEVA GALERÍA */}
        <div className="modal-seccion">
          <h3>Crear Galería</h3>
          <form onSubmit={handleCrearGaleria} className="modal-form-inline">
            <input 
              type="text" 
              placeholder="Nombre único (ej: Amor-Bustos)" 
              value={nuevaGaleria} 
              onChange={(e) => setNuevaGaleria(e.target.value)}
              className="modal-input"
              maxLength={25}
            />
            <button type="submit" className="btn-accion btn-modal" disabled={cargando}>
              Crear
            </button>
          </form>
          {mensajeGaleria.texto && (
            <div className={`modal-mini-alerta ${mensajeGaleria.tipo}`}>
              {mensajeGaleria.texto}
            </div>
          )}
        </div>

        {/* SECCIÓN 3: UNIRSE A GALERÍA EXISTENTE */}
        <div className="modal-seccion">
          <h3>Unirse a una Galería</h3>
          <p className="modal-explicacion">Escribe el nombre de la galería de tu amigo para enviarle una solicitud.</p>
          <form onSubmit={handleSolicitarUnirse} className="modal-form-inline">
            <input 
              type="text" 
              placeholder="Nombre exacto de la galería" 
              value={nombreBuscar} 
              onChange={(e) => setNombreBuscar(e.target.value)}
              className="modal-input"
            />
            <button type="submit" className="btn-accion btn-modal" disabled={cargando}>
              Unirse
            </button>
          </form>
          {mensajeBuscar.texto && (
            <div className={`modal-mini-alerta ${mensajeBuscar.tipo}`}>
              {mensajeBuscar.texto}
            </div>
          )}
        </div>

        {/* SECCIÓN 4: VINCULAR CUADRO FÍSICO */}
        <div ref={vincularRef} className={`modal-seccion ${resaltarVincular ? 'resaltado-pulsar' : ''}`}>
          <h3>Vincular Cuadro Físico</h3>
          <p className="modal-explicacion">Introduce el código PIN de 6 dígitos mostrado en la pantalla de tu cuadro.</p>
          <form onSubmit={handleVincularCuadro} className="modal-form-inline">
            <input 
              type="text" 
              placeholder="Código de 6 dígitos (ej: 729105)" 
              value={codigoPin} 
              onChange={(e) => setCodigoPin(e.target.value)}
              className="modal-input"
              maxLength={6}
            />
            <button type="submit" className="btn-accion btn-modal" disabled={cargando}>
              Vincular
            </button>
          </form>
          {mensajePin.texto && (
            <div className={`modal-mini-alerta ${mensajePin.tipo}`}>
              {mensajePin.texto}
            </div>
          )}
        </div>

        {/* SECCIÓN 5: SOLICITUDES DE UNIÓN PENDIENTES */}
        {solicitudes.length > 0 && (
          <div className="modal-seccion">
            <h3>Solicitudes Pendientes</h3>
            <p className="modal-explicacion">Amigos que desean unirse a tus galerías:</p>
            <div className="solicitudes-list">
              {solicitudes.map(s => (
                <div key={s.id} className="solicitud-item">
                  <div className="solicitud-detalles">
                    <strong>{s.usuario_nombre || s.usuario_username}</strong> quiere unirse a <em>{s.galeria_nombre}</em>
                  </div>
                  <div className="solicitud-acciones">
                    <button 
                      onClick={() => handleResponderSolicitud(s.id, 'aprobar')} 
                      className="btn-sol-aprobar"
                    >
                      Aceptar
                    </button>
                    <button 
                      onClick={() => handleResponderSolicitud(s.id, 'rechazar')} 
                      className="btn-sol-rechazar"
                    >
                      Rechazar
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default SettingsModal;
