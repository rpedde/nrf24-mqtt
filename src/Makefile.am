pkglibdir=$(libdir)/evgopherd
sbin_PROGRAMS = nrf24-mqtt


nrf24_mqtt_SOURCES = main.c nrf24-mqtt.h \
	 debug.c debug.h cfg.h cfg.c mqtt.c mqtt.h \
         nrf24-recv.h

if CRAZY
nrf24_mqtt_SOURCES += nrf24-crazyradio-recv.c
else
nrf24_mqtt_SOURCES += nrf24-bitbang-recv.c
endif
