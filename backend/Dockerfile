FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1

WORKDIR /app

# Copiar sólo requirements para aprovechar cache
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

# Copiar el resto del proyecto
COPY . .

# Recopilar archivos estáticos en build time
RUN python manage.py collectstatic --noinput

# Puerto será provisto por Railway (variable $PORT)
CMD gunicorn core.wsgi:application --bind 0.0.0.0:${PORT:-8000}