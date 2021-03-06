

ifneq ($(filter DRIVER_HYGROMETER_HIH5030,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_HYGROMETER
$(NAME)_COMPONENTS += libraries/drivers/hygrometers/hih5030
endif

ifneq ($(filter DRIVER_HYGROMETER_HIH6130,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_HYGROMETER
$(NAME)_COMPONENTS += libraries/drivers/hygrometers/hih6130
endif

ifneq ($(filter DRIVER_HYGROMETER_SI7021,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_HYGROMETER
$(NAME)_COMPONENTS += libraries/drivers/hygrometers/si7021
endif



ifneq ($(filter SENSOR_LIB_HYGROMETER,$(GLOBAL_DEFINES) $(PROCESSED_SDK_DEFINES)),)
GLOBAL_INCLUDES += types/hygrometer
endif