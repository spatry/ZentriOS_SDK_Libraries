

ifneq ($(filter DRIVER_ACCELEROMETER_LIS3DH,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_ACCELEROMETER
$(NAME)_COMPONENTS += libraries/drivers/accelerometers/lis3dh
endif

ifneq ($(filter DRIVER_ACCELEROMETER_BMI160,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_ACCELEROMETER
$(NAME)_COMPONENTS += libraries/drivers/accelerometers/bmi160
endif

ifneq ($(filter DRIVER_ACCELEROMETER_MPU9250,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_ACCELEROMETER
$(NAME)_COMPONENTS += libraries/drivers/accelerometers/mpu9250
endif

ifneq ($(filter DRIVER_ACCELEROMETER_FXOS8700CQ,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_ACCELEROMETER
$(NAME)_COMPONENTS += libraries/drivers/accelerometers/fxos8700cq
endif

ifneq ($(filter DRIVER_ACCELEROMETER_LSM6DS0,$(PROCESSED_SDK_DEFINES)),)
GLOBAL_DEFINES += SENSOR_LIB_ACCELEROMETER
$(NAME)_COMPONENTS += libraries/drivers/accelerometers/lsm6ds0
endif

ifneq ($(filter SENSOR_LIB_ACCELEROMETER,$(GLOBAL_DEFINES) $(PROCESSED_SDK_DEFINES)),)
GLOBAL_INCLUDES += types/accelerometer
endif