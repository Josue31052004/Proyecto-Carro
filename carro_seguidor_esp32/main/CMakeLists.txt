idf_component_register(SRCS "main.c" "tasks/motor.c" "tasks/sensor.c" "tasks/communication.c" 
                    INCLUDE_DIRS "tasks"
                    REQUIRES freertos driver esp_adc)
