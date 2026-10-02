# Firmware Huerta H² v2.15.1

La automatización queda dividida en dos subsistemas independientes:

- Climatización: `ventilation_enabled`, temperatura MIN/MAX y humedad MIN/MAX. Controla únicamente extractores y ventiladores.
- Dosificación: `dosing_enabled`, pH MIN/MAX, duración de pulso y espera. Controla únicamente las dos bombas dosificadoras.

Los dos grupos conservan sus valores independientemente en NVS. Las bombas continúan siendo mutuamente excluyentes y se fuerzan a OFF durante el arranque.
