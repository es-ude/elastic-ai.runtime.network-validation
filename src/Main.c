#define SOURCE_FILE "MAIN"

// internal headers
#include "AI.h"
#include "Common.h"
#include "Esp.h"
#include "Flash.h"
#include "FpgaConfigurationHandler.h"
#include "MqttBroker.h"
#include "Network.h"
#include "NetworkConfiguration.h"
#include "Protocol.h"
#include "enV5HwController.h"

// pico-sdk headers
#include "hardware/spi.h"
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>

// external headers
#include <malloc.h>
#include <string.h>

/* region VARIABLES/DEFINES */

//Changes these number to your needs
size_t num_inputs = 20;
size_t num_results = 10;

// Flash
spi_t spiToFlash = {.spi = spi0, .baudrate = 5000000, .sckPin = 2, .mosiPin = 3, .misoPin = 0};
uint8_t flashChipSelectPin = 1;

// MQTT
char *twinID;
bool subscribed = false;

typedef struct downloadRequest {
    char *url;
    size_t fileSizeInBytes;
    size_t startAddress;
} downloadRequest_t;
downloadRequest_t *downloadRequest = NULL;

/* endregion VARIABLES/DEFINES */

void init(void) {
    // First check if we crash last time -> reboot into boot rom mode
    if (watchdog_enable_caused_reboot()) {
        reset_usb_boot(0, 0);
    }

    // init usb
    stdio_init_all();

    // waits for usb connection, REMOVE to continue without waiting for connection
    while ((!stdio_usb_connected())) {}

    // Checks connection to ESP and initialize ESP
    espInit();

    // initialize WiFi and MQTT broker
    networkTryToConnectToNetworkUntilSuccessful(networkCredentials);
    mqttBrokerConnectToBrokerUntilSuccessful(mqttHost, "eip://uni-due.de/es", "enV5");

    // initialize FPGA and flash
    flashInit(&spiToFlash, flashChipSelectPin);
    env5HwInit();
    fpgaConfigurationHandlerInitialize();
}

void receiveDownloadBinRequest(posting_t posting) {
    // get download request
    PRINT("Received Download Bin Request");
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
    PRINT("Received download data as URL: %s, fileSizeInBytes: %s,startAddress: %s", url, length, position);
    downloadRequest = malloc(sizeof(downloadRequest_t));
    downloadRequest->url = url;
    downloadRequest->fileSizeInBytes = length;
    downloadRequest->startAddress = position;
}

void waitAndDownloadFpgaConfig(void) {
    /* NOTE:
     *   1. add listener for download start command (MQTT)
     *      uart handle should only set flag -> download handled at task
     *   2. download data from server and stored to flash
     *   4. add listener for FPGA flashing command
     *   5. trigger flash of FPGA
     *      handled in UART interrupt
     */

    sleep_ms(2000);
    protocolSubscribeForCommand("FLASH",
                                (subscriber_t){.deliver = receiveDownloadBinRequest});

    PRINT("FPGA Ready ...")

    while (downloadRequest == NULL) {}

    env5HwFpgaPowersOff();

    PRINT_DEBUG("Download: position in flash: %i, address: %s, size: %i",
                downloadRequest->startAddress, downloadRequest->url,
                downloadRequest->fileSizeInBytes)

    fpgaConfigurationHandlerError_t configError =
        fpgaConfigurationHandlerDownloadConfigurationViaHttp(
            downloadRequest->url, downloadRequest->fileSizeInBytes, downloadRequest->startAddress);

    if (configError != FPGA_RECONFIG_NO_ERROR) {
        protocolPublishCommandResponse("FLASH", false);
        PRINT("ERASE ERROR")
    } else {
        sleep_ms(10);
        env5HwFpgaPowersOn();
        PRINT("FPGA reconfigured")
        sleep_ms(2000);
        // todo Receive Accelerator ID through MQTT and compare
        if (AI_deploy(downloadRequest->startAddress, 47)) {
            PRINT("FPGA deployed");
            protocolPublishCommandResponse("FLASH", true);
        }
    }

    // clean artifacts
    free(downloadRequest->url);
    free(downloadRequest);
    downloadRequest = NULL;
    PRINT("Download finished!")
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

_Noreturn void mainloop(void) {
    while (1) {}
}

int main() {
    // initialize hardware
    init();
    waitAndDownloadFpgaConfig();
    subscribeComputeTopic();
    mainloop();
}
