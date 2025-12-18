/*
    main.c -- Main for demo app on FreeRTOS

    This app demonstrates Ioto data synchronization to the cloud
 */
 #include "ioto.h"
 #include "FreeRTOS.h"
 #include "task.h"
 
 /*
     Ioto main task. Runs on the Ioto main fiber (coroutine)
  */
 static void TaskMain(void *arg) {
     chdir("./ioto");
     ioStartRuntime(IOTO_VERBOSE);
     ioRun(ioStart);
     ioStopRuntime();
 }
 
 /*
     Demo main
  */
 int main(void) {
     xTaskCreate(TaskMain, "Ioto", ME_FIBER_INITIAL_STACK, NULL, tskIDLE_PRIORITY + 1, NULL);
     vTaskStartScheduler();
     return 0;
 }
 
 /*
     Copyright (c) Embedthis Software. All Rights Reserved.
     This is proprietary software and requires a commercial license from the author.
  */
 