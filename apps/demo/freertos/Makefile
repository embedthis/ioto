#
# 	Minimal Makefile for Ioto on FreeRTOS POSIX simulator
#
CC                    := gcc
BIN                   := demo
BUILD_DIR             := ./build
BUILD_DIR_ABS         := $(abspath $(BUILD_DIR))

#
# 	Directories
#
FREERTOS_DIR          := $(abspath ../../../FreeRTOS)
FREERTOS_PLUS_DIR     := $(abspath ../../../FreeRTOS-Plus)
KERNEL_DIR            := ${FREERTOS_DIR}/Source
IOTO_DIR              := ioto
MBEDTLS_DIR           := ${FREERTOS_PLUS_DIR}/ThirdParty/mbedtls

#
# 	Include paths
#
INCLUDE_DIRS          := -I.
INCLUDE_DIRS          += -I${KERNEL_DIR}/include
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils
INCLUDE_DIRS          += -I${IOTO_DIR}/include
INCLUDE_DIRS          += -I${MBEDTLS_DIR}/include

#
# 	Source files
#
SOURCE_FILES          := $(wildcard *.c)
SOURCE_FILES          += $(wildcard ${FREERTOS_DIR}/Source/*.c)
SOURCE_FILES          += ${KERNEL_DIR}/portable/MemMang/heap_3.c
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/port.c
SOURCE_FILES          += $(wildcard ${IOTO_DIR}/lib/*.c ${IOTO_DIR}/src/*.c ${IOTO_DIR}/src/cloud/*.c)
SOURCE_FILES          += $(wildcard ${MBEDTLS_DIR}/library/*.c)
ASM_FILES             := $(wildcard ${IOTO_DIR}/lib/*.S)

#
# 	Compiler and linker flags
#
CFLAGS                := -g -O0
CFLAGS                += -DME_OS_TYPE=19 -DME_COM_MBEDTLS=1 -DME_COM_OPENSSL=0 -DAPP=demo
LDFLAGS               := -g -pthread
CPPFLAGS              := $(INCLUDE_DIRS)
CPPFLAGS              += -DBUILD_DIR=\"$(BUILD_DIR_ABS)\"

#
# 	Build objects
#
OBJ_FILES              = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)
OBJ_FILES              += $(ASM_FILES:%.S=$(BUILD_DIR)/%.o)
DEP_FILE               = $(OBJ_FILES:%.o=%.d)

#
# 	Targets
#
${BIN}: $(BUILD_DIR)/$(BIN)

${BUILD_DIR}/${BIN}: ${OBJ_FILES}
	@mkdir -p ${@D}
	$(CC) $^ ${LDFLAGS} -o $@

-include ${DEP_FILE}

${BUILD_DIR}/%.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c $< -o $@

${BUILD_DIR}/%.o: %.S Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c $< -o $@

.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)
