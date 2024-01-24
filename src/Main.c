#define SOURCE_FILE "MAIN"

// internal headers
#include "AI.h"
#include "Common.h"
#include "Esp.h"
#include "Flash.h"
#include "FpgaConfigurationHandler.h"
#include "MqttBroker.h"
#include "Network.h"
#include "Protocol.h"
#include "enV5HwController.h"
#include "FreeRtosTaskWrapper.h"

// pico-sdk headers
#include "hardware/spi.h"
#include "middleware.h"
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>

// external headers
#include <malloc.h>
#include <string.h>

/* region VARIABLES/DEFINES */

extern networkCredentials_t networkCredentials;
extern mqttBrokerHost_t mqttHost;


//Changes these number to your needs
size_t num_inputs = 20;
size_t num_results = 10;

// Flash
spi_t spiToFlash = {.spi = spi0, .baudrate = 5000000, .sckPin = 2, .mosiPin = 3, .misoPin = 0};
uint8_t flashChipSelectPin = 1;

// MQTT
char *twinID;
bool subscribed = false;
bool flashing = false;

typedef struct {
    char *url;
    size_t fileSizeInBytes;
    size_t startSectorId;
} downloadRequest_t;
downloadRequest_t *downloadRequest = NULL;

/* endregion VARIABLES/DEFINES */

void init(void) {
    // Should always be called first thing to prevent unique behavior, like current leakage
    env5HwInit();

    // Connect to Wi-Fi network and MQTT Broker
    espInit();
    networkTryToConnectToNetworkUntilSuccessful(networkCredentials);
    mqttBrokerConnectToBrokerUntilSuccessful(mqttHost, "eip://uni-due.de/es", "enV5");

    /* Always release flash after use:
     *   -> FPGA and MCU share the bus to flash-memory.
     * Make sure this is only enabled while FPGA does not use it and release after use before
     * powering on, resetting or changing the configuration of the FPGA.
     * FPGA needs that bus during reconfiguration and **only** during reconfiguration.
     */
    flashInit(&spiToFlash, flashChipSelectPin);
    fpgaConfigurationHandlerInitialize();
    env5HwFpgaPowersOff();

    // initialize the serial output
    stdio_init_all();
    while ((!stdio_usb_connected())) {
        // wait for serial connection
    }
}

void receiveDownloadBinRequest(posting_t posting) {
    PRINT("RECEIVED FLASH REQUEST")
    // get download request
    char *urlStart = strstr(posting.data, "URL:") + 4;
    char *urlEnd = strstr(urlStart, ";") - 1;
    size_t urlLength = urlEnd - urlStart + 1;
    char *url = malloc(urlLength);
    memcpy(url, urlStart, urlLength);
    url[urlLength - 1] = '\0';
    char *sizeStart = strstr(posting.data, "SIZE:") + 5;
    char *endSize = strstr(sizeStart, ";") - 1;
    size_t length = strtol(sizeStart, &endSize, 10);

    char *positionStart = strstr(posting.data, "POSITION:") + 9;
    char *positionEnd = strstr(positionStart, ";") - 1;
    size_t position = strtol(positionStart, &positionEnd, 10);

    downloadRequest = malloc(sizeof(downloadRequest_t));
    downloadRequest->url = url;
    downloadRequest->fileSizeInBytes = length;
    downloadRequest->startSectorId = position;
}

_Noreturn void waitAndDownloadFpgaConfig(void) {
    /* NOTE:
     *   1. add listener for download start command (MQTT)
     *      uart handle should only set flag -> download handled at task
     *   2. download data from server and stored to flash
     *   4. add listener for FPGA flashing command
     *   5. trigger flash of FPGA
     *      handled in UART interrupt
     */

    freeRtosTaskWrapperTaskSleep(2000);
    protocolSubscribeForCommand("FLASH",
                                (subscriber_t){.deliver = receiveDownloadBinRequest});
    publishAliveStatusMessage("");

    PRINT("FPGA Ready ...")
    while (true) {
        if (downloadRequest == NULL) {
            freeRtosTaskWrapperTaskSleep(1000);
            continue;
        }
        flashing = true;

        PRINT("Starting to download bitfile...")

        env5HwFpgaPowersOff();

        PRINT_DEBUG("Download: position in flash: %i, address: %s, size: %i",
                    downloadRequest->startSectorId, downloadRequest->url,
                    downloadRequest->fileSizeInBytes)

        fpgaConfigurationHandlerError_t configError =
            fpgaConfigurationHandlerDownloadConfigurationViaHttp(downloadRequest->url,
                                                                 downloadRequest->fileSizeInBytes,
                                                                 downloadRequest->startSectorId);

        // clean artifacts
        free(downloadRequest->url);
        free(downloadRequest);
        downloadRequest = NULL;
        PRINT("Download finished!")

        PRINT("Try reconfigure FPGA")
        if (configError != FPGA_RECONFIG_NO_ERROR) {
            protocolPublishCommandResponse("FLASH", false);
            PRINT("ERASE ERROR")
        } else {
            freeRtosTaskWrapperTaskSleep(10);
            env5HwFpgaPowersOn();
            freeRtosTaskWrapperTaskSleep(500);
            if (!AI_deploy(downloadRequest->startSectorId, 47)) {
                PRINT("Deploy failed!")
                protocolPublishCommandResponse("FLASH", false);
            }
            else {
                PRINT("FPGA reconfigured")
                protocolPublishCommandResponse("FLASH", true);
            }
            flashing = false;
            freeRtosTaskWrapperTaskSleep(10000);

        }
    }
}

void receiveDataToCompute(posting_t posting) {
    // extract number of values
    int8_t inputs[num_inputs];

    // split string into substrings
    char *dataToComputeSplit = strtok(posting.data, ";");
    size_t index = 0;
    while (dataToComputeSplit != NULL) {
        inputs[index] = (int8_t)strtol(dataToComputeSplit, NULL, 10);
        dataToComputeSplit = strtok(NULL, ";");
    }
    // compute results

    int8_t results[num_results];
    AI_predict(inputs, num_inputs, results, num_results);

    // convert results to string to publish them via MQTT! Todo fuck with encodings
    char *resultsToPublish = malloc(sizeof(char) * 5 * num_results);
    resultsToPublish[0] = '\0';
    for (int i = 0; i < num_results; i++) {
        char result[5];
        itoa(results[i], result, 10);
        strncat(resultsToPublish, result, 5);
        strncat(resultsToPublish, ";", 2);
    }

    // publish results
    protocolPublishData("results", resultsToPublish);
}

void subscribeComputeTopic() {
    protocolSubscribeForCommand("inputs", (subscriber_t){.deliver = receiveDataToCompute});
}


int main() {
    // initialize hardware
    init();
    sleep_ms(2000);
    env5HwFpgaPowersOn();
    PRINT("FPGA powered on");
    sleep_ms(56000);
    PRINT("FPGA reconfigured?!")
    uint8_t led_status;
    while (1){
        middlewareSetFpgaLeds(0xff);
        sleep_ms(1000);
        led_status = middlewareGetLeds();
        PRINT("%i", led_status);
        sleep_ms(1000);
        middlewareSetFpgaLeds(0x00);
        sleep_ms(1000);
        led_status = middlewareGetLeds();
        PRINT("%i", led_status);
        sleep_ms(1000);
    }

    //freeRtosTaskWrapperRegisterTask(waitAndDownloadFpgaConfig,"fpgaDownloadAndFlash", 0, FREERTOS_CORE_0);
    //subscribeComputeTopic();
    //freeRtosTaskWrapperStartScheduler();
}
