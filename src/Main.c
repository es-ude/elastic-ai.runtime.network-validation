#define SOURCE_FILE "MAIN"

// internal headers
#include "Common.h"
#include "Esp.h"
#include "Flash.h"
#include "enV5HwController.h"
#include "MqttBroker.h"
#include "Network.h"
#include "NetworkConfiguration.h"
#include "FpgaConfigurationHandler.h"
#include "Protocol.h"

// pico-sdk headers
#include <hardware/watchdog.h>
#include "hardware/spi.h"
#include <pico/bootrom.h>
#include <pico/stdlib.h>

// external headers
#include <string.h>
#include <malloc.h>

/* region VARIABLES/DEFINES */

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

//TODO find out, how FPGA "calculates" value and how to access it
char *fgpaReturnValue;

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
    downloadRequest->startAddress = position;
}

void computeData(void) {
    /* NOTE:
     *   1. add listener for download start command (MQTT)
     *      uart handle should only set flag -> download handled at task
     *   2. download data from server and stored to flash
     *   4. add listener for FPGA flashing command
     *   5. trigger flash of FPGA
     *      handled in UART interrupt
     */

    sleep_ms(2000);
    protocolSubscribeForCommand("compute-data", (subscriber_t){.deliver = receiveDownloadBinRequest});

    PRINT("FPGA Ready ...")

    while (downloadRequest == NULL) {}

    env5HwFpgaPowersOff();

    PRINT_DEBUG("Download: position in flash: %i, address: %s, size: %i",
                downloadRequest->startAddress, downloadRequest->url,
                downloadRequest->fileSizeInBytes)

    fpgaConfigurationHandlerError_t configError =
        fpgaConfigurationHandlerDownloadConfigurationViaHttp(downloadRequest->url,
                                                             downloadRequest->fileSizeInBytes,
                                                             downloadRequest->startAddress);

    // clean artifacts
    free(downloadRequest->url);
    free(downloadRequest);
    downloadRequest = NULL;
    PRINT("Download finished!")

    if (configError != FPGA_RECONFIG_NO_ERROR) {
        protocolPublishCommandResponse("compute-data", false);
        PRINT("ERASE ERROR")
    } else {
        sleep_ms(10);
        env5HwFpgaPowersOn();
        PRINT("FPGA reconfigured")
        protocolPublishCommandResponse("compute-data", true);
    }
}

void publishData(char *dataID) {
    protocolPublishData(dataID, fgpaReturnValue);
}

int main() {
    // initialize hardware
    init();

    // subscribe to "compute-data" topic and configure FPGA from received .bin file
    computeData();

    // publishes calculated value by FPGA via MQTT to topic "computed-result"
    publishData("computed-result");
}



