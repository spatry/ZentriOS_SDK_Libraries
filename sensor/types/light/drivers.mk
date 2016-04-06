
ifneq ($(filter DRIVER_LIGHT_OPT3001,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_LIGHT
$(NAME)_COMPONENTS += libraries/drivers/lights/opt3001
endif

ifneq ($(filter SENSOR_LIB_LIGHT,$(GLOBAL_DEFINES) $(PROCESSED_SDK_DEFINES)),)
GLOBAL_INCLUDES += types/light
endif