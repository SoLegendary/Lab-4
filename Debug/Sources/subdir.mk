################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/accel.c \
../Sources/main.c 

OBJS += \
./Sources/accel.o \
./Sources/main.o 

C_DEPS += \
./Sources/accel.d \
./Sources/main.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/%.o: ../Sources/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -I"C:\Users\11988401\Desktop\Lab-4-Workspace\Lab-4-initial\Library" -I"C:/Users/11988401/Desktop/Lab-4-Workspace/Lab-4-initial/Static_Code/IO_Map" -I"C:/Users/11988401/Desktop/Lab-4-Workspace/Lab-4-initial/Sources" -I"C:/Users/11988401/Desktop/Lab-4-Workspace/Lab-4-initial/Generated_Code" -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


