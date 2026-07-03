import { useState, useEffect } from 'react';
import { getFotos, subirFoto, eliminarFoto, mostrarFotoEnCuadros, getGalerias, getMisMarcos, seleccionarGaleriaActiva } from './api/galeria';
import AniversarySplash from './AniversarySplash';
import AuthScreen from './components/AuthScreen';
import SettingsModal from './components/SettingsModal';
import './App.css';

function App() {
  // 1. Estados
  const [fotos, setFotos] = useState([]);
  const [indiceActual, setIndiceActual] = useState(0);
  const [mostrarSplash, setMostrarSplash] = useState(true);
  const [usuario, setUsuario] = useState(localStorage.getItem('usuario_cuadrito') || 'Mi Amor');
  const [token, setToken] = useState(localStorage.getItem('token_cuadrito') || null);
  const [activeGaleriaId, setActiveGaleriaId] = useState(null);
  const [mostrarAjustes, setMostrarAjustes] = useState(false);
  const [marcos, setMarcos] = useState([]);
  const [resaltarSeccionVincular, setResaltarSeccionVincular] = useState(false);
  const [galerias, setGalerias] = useState([]);

  // 2. Carga inicial
  useEffect(() => {
    if (token) {
      cargarFotosDesdeBackend();
      cargarMarcos();
    }
  }, [token, activeGaleriaId]);

  const cargarMarcos = async () => {
    try {
      const data = await getMisMarcos();
      setMarcos(data);
    } catch (error) {
      console.error("Error al cargar los marcos del usuario:", error);
    }
  };

  // Polling automático cada 5 segundos para actualizar fotos en tiempo real sin recargar la página
  useEffect(() => {
    if (!token || !activeGaleriaId) return;
    const interval = setInterval(() => {
      cargarFotosSilencioso();
    }, 5000);
    return () => clearInterval(interval);
  }, [token, activeGaleriaId]);

  const cargarFotosSilencioso = async () => {
    if (!token || !activeGaleriaId) return;
    try {
      const data = await getFotos(activeGaleriaId);
      setFotos((prevFotos) => {
        const coinciden = prevFotos.length === data.length && 
                          prevFotos.every((f, i) => f.id === data[i].id);
        return coinciden ? prevFotos : data;
      });
    } catch (error) {
      console.error("Error al actualizar fotos en segundo plano:", error);
    }
  };

  const handleLoginSuccess = (newToken, newName) => {
    setToken(newToken);
    setUsuario(newName);
    setActiveGaleriaId(null); // Resetear para que cargue la lista en el primer useEffect
  };

  const handleLogout = () => {
    localStorage.removeItem('token_cuadrito');
    setToken(null);
    setActiveGaleriaId(null);
    setMarcos([]);
    setMostrarAjustes(false);
    setResaltarSeccionVincular(false);
  };

  const handleGaleriaChange = async (newGalId) => {
    try {
      if (newGalId) {
        await seleccionarGaleriaActiva(newGalId);
      }
      setActiveGaleriaId(newGalId);
      setIndiceActual(0);
    } catch (error) {
      console.error("Error al cambiar la galería activa en el servidor:", error);
    }
  };

  const cargarFotosDesdeBackend = async () => {
    try {
      const list = await getGalerias();
      setGalerias(list);
      
      let currentGalId = activeGaleriaId;
      if (!currentGalId) {
        if (list.length > 0) {
          currentGalId = list[0].id;
          setActiveGaleriaId(currentGalId);
        }
      }
      if (currentGalId) {
        const data = await getFotos(currentGalId);
        setFotos(data);
      } else {
        setFotos([]);
      }
    } catch (error) {
      console.error("Error al cargar las fotos:", error);
    }
  };

  // 3. Funciones de navegación
  const irFotoAnterior = () => {
    setIndiceActual((prev) => (prev === 0 ? fotos.length - 1 : prev - 1));
  };

  const irFotoSiguiente = () => {
    setIndiceActual((prev) => (prev === fotos.length - 1 ? 0 : prev + 1));
  };

  // Función matemática vital que probablemente faltaba
  const getIndiceRelativo = (desplazamiento) => {
    if (fotos.length === 0) return null;
    return (indiceActual + desplazamiento + fotos.length) % fotos.length;
  };

  // 4. Acciones
  const handleEliminar = async () => {
    if (fotos.length === 0) return;
    const fotoActual = fotos[indiceActual];
    const confirmar = window.confirm("¿Seguro que quieres borrar esta foto de AWS y del marco?");
    
    if (confirmar) {
      try {
        await eliminarFoto(fotoActual.id);
        await cargarFotosDesdeBackend();
        setIndiceActual(0);
      } catch (error) {
        console.error("Error al eliminar:", error);
      }
    }
  };

  const handleMostrarEnCuadros = async () => {
    if (fotos.length === 0) return;
    const fotoActual = fotos[indiceActual];
    try {
      await mostrarFotoEnCuadros(fotoActual.id, usuario);
      // Notificación discreta flotante
      const notificacion = document.createElement('div');
      notificacion.className = 'alerta-flotante';
      notificacion.innerText = 'Sincronizando imagen en los cuadros...';
      document.body.appendChild(notificacion);
      setTimeout(() => notificacion.remove(), 3000);
    } catch (error) {
      console.error("Error al mostrar la foto en los cuadros:", error);
    }
  };

  const handleSubirFoto = async (evento) => {
    const archivo = evento.target.files[0];
    if (!archivo) return;
    const formData = new FormData();
    formData.append('imagen', archivo);
    formData.append('usuario', usuario);
    if (activeGaleriaId) {
      formData.append('galeria', activeGaleriaId);
    }
    try {
      await subirFoto(formData);
      await cargarFotosDesdeBackend();
    } catch (error) {
      console.error("Error al subir la foto:", error);
    }
  };

  const finalizarSplash = () => {
    setMostrarSplash(false);
  };

  // 5. La Interfaz (El condicional maestro)
  return (
    <>
      {mostrarSplash ? (
        <AniversarySplash alTerminar={finalizarSplash} />
      ) : !token ? (
        <AuthScreen onLoginSuccess={handleLoginSuccess} />
      ) : (
        <div className="app-container">
          <div className="cabecera-app">
            <button onClick={() => setMostrarAjustes(true)} className="btn-ajustes">
              ⚙ Ajustes
            </button>
            <button onClick={handleLogout} className="btn-logout">
              Cerrar Sesión ✕
            </button>
          </div>
          {marcos.length === 0 && (
            <div 
              className="alerta-vinculacion-banner" 
              onClick={() => {
                setResaltarSeccionVincular(true);
                setMostrarAjustes(true);
              }}
            >
              <span className="corazon-animado-mini">💖</span>
              <span>¡Vincula tu primer cuadro! Haz clic aquí para configurar tu pantalla física.</span>
              <span className="flecha-banner">➔</span>
            </div>
          )}
          <h1 className="titulo">NUESTRO CUADRITO</h1>
          <h2 className="subtitulo">
            Aquí puedes ver las fotos que tenemos
          </h2>
          
          {fotos.length > 0 ? (
            <div className="carrusel-3d-escena">
              <button onClick={irFotoAnterior} className="btn-flecha btn-flecha-izq">
                &lt;
              </button>

              {fotos.map((foto, index) => {
                let clasePosicion = "foto-oculta";

                if (index === indiceActual) {
                  clasePosicion = "foto-frente";
                } else if (index === getIndiceRelativo(-1)) {
                  clasePosicion = "foto-detras foto-izquierda";
                } else if (index === getIndiceRelativo(1)) {
                  clasePosicion = "foto-detras foto-derecha";
                }

                return (
                  <div key={foto.id} className={`foto-tarjeta ${clasePosicion}`}>
                    <img src={foto.imagen} alt="Foto" className="marco-imagen-3d" />
                  </div>
                );
              })}

              <button onClick={irFotoSiguiente} className="btn-flecha btn-flecha-der">
                &gt;
              </button>
            </div>
          ) : (
            <p className="sin-fotos">
              No hay fotos aún. ¡Sube la primera!
            </p>
          )}

          <div className="panel-controles">
            <div className="seccion-galeria-activa">
              <span className="corazon-indicador">💝</span>
              <span className="texto-galeria-activa">Galería Activa: <strong>{galerias.find(g => g.id === activeGaleriaId)?.nombre || "Ninguna"}</strong></span>
            </div>

            <div className="grupo-acciones">
              <button 
                onClick={handleMostrarEnCuadros} 
                className="btn-accion btn-mostrar"
                disabled={fotos.length === 0}
              >
                Mostrar en Pantalla
              </button>

              <input type="file" accept="image/*" id="subir-foto" style={{ display: 'none' }} onChange={handleSubirFoto} />
              <label htmlFor="subir-foto" className="btn-accion btn-agregar">
                Subir Foto
              </label>

              <button 
                onClick={handleEliminar} 
                className="btn-accion btn-eliminar"
                disabled={fotos.length === 0}
              >
                Eliminar Foto
              </button>
            </div>
          </div>
        </div>
      )}
      {mostrarAjustes && (
        <SettingsModal 
          onClose={() => {
            setMostrarAjustes(false);
            setResaltarSeccionVincular(false);
          }} 
          activeGaleriaId={activeGaleriaId} 
          onGaleriaChange={handleGaleriaChange} 
          resaltarVincular={resaltarSeccionVincular}
          onVincularSuccess={cargarMarcos}
          marcos={marcos}
          cargarMarcos={cargarMarcos}
      )}
    </>
  );
}


export default App;