
ifneq ($(filter DRIVER_AIRQUALITY_VZ89,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_AIRQUALITY
$(NAME)_COMPONENTS += libraries/drivers/airquality/vz89
endif

ifneq ($(filter SENSOR_LIB_AIRQUALITY,$(GLOBAL_DEFINES) $(PROCESSED_SDK_DEFINES)),)
GLOBAL_INCLUDES += types/airquality
endif