/*
    FreeRTOSConfig.h -- Minimal FreeRTOS configuration for Ioto demo
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// Core scheduler settings
#define configUSE_PREEMPTION                       1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0
#define configTICK_RATE_HZ                         ( 1000 )
#define configMAX_PRIORITIES                       ( 7 )
#define configMINIMAL_STACK_SIZE                   ( PTHREAD_STACK_MIN )
#define configMAX_TASK_NAME_LEN                    ( 16 )
#define configUSE_16_BIT_TICKS                     0
#define configIDLE_SHOULD_YIELD                    1
#define configSTACK_DEPTH_TYPE                     uint32_t

// Memory allocation - dynamic only
#define configTOTAL_HEAP_SIZE                      ( ( size_t ) ( 65 * 1024 ) )
#define configSUPPORT_STATIC_ALLOCATION            0
#define configSUPPORT_DYNAMIC_ALLOCATION           1

// Synchronization primitives needed by Ioto
#define configUSE_MUTEXES                          1
#define configUSE_RECURSIVE_MUTEXES                1
#define configUSE_COUNTING_SEMAPHORES              1
#define configUSE_TASK_NOTIFICATIONS               1

// Timers needed by Ioto
#define configUSE_TIMERS                           1
#define configTIMER_TASK_PRIORITY                  ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                   10
#define configTIMER_TASK_STACK_DEPTH               ( configMINIMAL_STACK_SIZE * 2 )

// Disable all hooks - not needed
#define configUSE_IDLE_HOOK                        0
#define configUSE_TICK_HOOK                        0
#define configUSE_DAEMON_TASK_STARTUP_HOOK         0
#define configUSE_MALLOC_FAILED_HOOK               0
#define configCHECK_FOR_STACK_OVERFLOW             0

// Disable unused features
#define configUSE_TRACE_FACILITY                   0
#define configGENERATE_RUN_TIME_STATS              0
#define configUSE_CO_ROUTINES                      0
#define configUSE_QUEUE_SETS                       0
#define configUSE_APPLICATION_TASK_TAG             0
#define configQUEUE_REGISTRY_SIZE                  0

// Minimal API inclusions
#define INCLUDE_vTaskPrioritySet                   0
#define INCLUDE_uxTaskPriorityGet                  0
#define INCLUDE_vTaskDelete                        1
#define INCLUDE_vTaskSuspend                       1
#define INCLUDE_vTaskDelayUntil                    1
#define INCLUDE_vTaskDelay                         1
#define INCLUDE_xTaskGetSchedulerState             0
#define INCLUDE_xTaskGetCurrentTaskHandle          1
#define INCLUDE_uxTaskGetStackHighWaterMark        0
#define INCLUDE_xTaskGetIdleTaskHandle             0
#define INCLUDE_eTaskGetState                      0
#define INCLUDE_xTimerPendFunctionCall             0
#define INCLUDE_xTaskAbortDelay                    0

#endif /* FREERTOS_CONFIG_H */
