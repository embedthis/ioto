#
#   Makefile - manual probe against a real MQTT broker
#
#   The automated suite does not use this file. It runs under TestMe against the local broker that
#   setup.sh starts, or against the in-process fakebroker.h harness, and needs no credentials:
#
#       tm                  # the whole suite
#       tm connect          # one test
#
#   This target is for hand-checking the client against a real TLS broker, which needs a device
#   credential. Supply one from the environment; there are no defaults, and nothing is committed
#   here. A credential belongs to whoever is running the probe, not to the repository.
#
#       MQTT_HOST=xxxxxxxx-ats.iot.<region>.amazonaws.com \
#       MQTT_CA=/path/to/AmazonRootCA1.pem \
#       MQTT_CERT=/path/to/device.crt \
#       MQTT_KEY=/path/to/device.key \
#       MQTT_CLIENT=my-client-id \
#       make probe
#
PROBE_VARS := MQTT_HOST MQTT_CA MQTT_CERT MQTT_KEY MQTT_CLIENT
MQTT_TOPIC ?= test/topic

probe:
	@missing=""; for v in $(PROBE_VARS); do \
		eval "val=\$$$$v"; \
		if [ -z "$$val" ] ; then missing="$$missing $$v" ; fi ; \
	done ; \
	if [ -n "$$missing" ] ; then \
		echo "make probe: set$$missing - see the comment at the top of this Makefile" >&2 ; \
		exit 1 ; \
	fi
	pubsub -v --ca "$(MQTT_CA)" --cert "$(MQTT_CERT)" --key "$(MQTT_KEY)" \
		--host "$(MQTT_HOST)" --client "$(MQTT_CLIENT)" --topic "$(MQTT_TOPIC)"

.PHONY: probe
