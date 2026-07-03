import React, { useEffect, useState, useMemo } from 'react';
import CorazonSVG from './assets/corazon.svg'; 
import './AniversarySplash.css'; 

const AniversarySplash = ({ alTerminar }) => {
  const [mostrarTexto, setMostrarTexto] = useState(false);

  const corazonesArr = useMemo(() => {
    const numCorazones = 80; 
    const arr = [];
    for (let i = 0; i < numCorazones; i++) {
      arr.push({
        id: i,
        top: Math.random() * 100 + '%',
        left: Math.random() * 100 + '%',
        size: 40 + Math.random() * 50 + 'px',
        rotation: -20 + Math.random() * 40 + 'deg',
        duracion: 0.5 + Math.random() * 0.7 + 's',
        retraso: Math.random() * 3 + 's'
      });
    }
    return arr;
  }, []);

  useEffect(() => {
    const timerTexto = setTimeout(() => {
      setMostrarTexto(true);
    }, 500);

    return () => clearTimeout(timerTexto);
  }, []); 

  return (
    <div className="splash-escena">
      {corazonesArr.map((corazon) => (
        <img
          key={corazon.id}
          src={CorazonSVG}
          alt="Corazón decorativo"
          className="corazon-pulsante"
          style={{
            top: corazon.top,
            left: corazon.left,
            width: corazon.size,
            height: corazon.size,
            transform: `rotate(${corazon.rotation})`,
            animationDuration: corazon.duracion,
            animationDelay: corazon.retraso,
          }}
        />
      ))}

      <div className=""></div>

      {/* NUEVO: Contenedor central que anima el texto y el botón juntos */}
      <div className={`contenedor-mensaje ${mostrarTexto ? 'visible' : ''}`}>
        <h1 className="mensaje-aniversario ">
          FELIZ ANIVERSARIO AMORCITO
        </h1>
        <button className="btn-entrar" onClick={alTerminar}>
          Presioname para ver tu regalo ❤️
        </button>
      </div>
    </div>
  );
};

export default AniversarySplash;