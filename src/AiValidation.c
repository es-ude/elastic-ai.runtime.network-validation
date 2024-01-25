/* IMPORTANT: This program only works with the logic design with middleware!
 *
 * IMPORTANT: To reach access the wifi-network the network credentials have to be updated!
 *
 * NOTE: To run this test, a server that serves the HTTPGet request is required.
 *       This server can be started by running the `bitfile_http_server.py` script
 *       in the `bitfile_scripts` folder.
 *       After starting the server, it shows an IP-address and port where it can be reached.
 *       This IP address needs to be used for the `baseUrl` field in your NetworkConfiguration.c.
 *
 * NOTE: If you update the echo_server binary file you have to update the `configSize` field
 *       with the correct size of the file in bytes.
 *       This size can be determined by running `du -b <path_to_file>`.
 */

#define SOURCE_FILE "MIDDLEWARE-HWTEST"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/spi.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"

#include "AI.h"
#include "Common.h"
#include "Esp.h"
#include "Flash.h"
#include "FpgaConfigurationHandler.h"
#include "HTTP.h"
#include "Network.h"
#include "RestMethods.h"
#include "Sleep.h"
#include "Spi.h"
#include "enV5HwController.h"
#include "middleware.h"

extern networkCredentials_t networkCredentials;

spi_t spiConfiguration = {
    .spi = spi0, .baudrate = 5000000, .misoPin = 0, .mosiPin = 3, .sckPin = 2};
uint8_t csPin = 1;

uint32_t sectorIdForConfig = 1;


static void initHardware(void) {
    // Should always be called first thing to prevent unique behavior, like current leakage
    env5HwInit();

    /* Always release flash after use:
     *   -> FPGA and MCU share the bus to flash-memory.
     * Make sure this is only enabled while FPGA does not use it and release after use before
     * powering on, resetting or changing the configuration of the FPGA.
     * FPGA needs that bus during reconfiguration and **only** during reconfiguration.
     */
    flashInit(&spiConfiguration, csPin);

    // initialize the serial output
    stdio_init_all();
    while ((!stdio_usb_connected())) {
        // wait for serial connection
    }
}

static void connectToWifi(void) {
    espInit();
    PRINT("Try Connecting to WiFi")
    networkTryToConnectToNetworkUntilSuccessful(networkCredentials);
}

static void downloadBinFile(void) {
    PRINT("Downloading HW configuration...")
    int file_length  = getIntViaRest("/binfile_length");
    char url[256];
    buildRestCallUrl("/binfile", url, sizeof(url));
    fpgaConfigurationHandlerError_t error =
        fpgaConfigurationHandlerDownloadConfigurationViaHttp(url, file_length, sectorIdForConfig);
    if (error != FPGA_RECONFIG_NO_ERROR) {
        PRINT("Download failed!")
        sleep_for_ms(3000);
    }
    PRINT("Download Successful.")
}

static int8_t* downloadDataToCompute(){

    int data_length = getIntViaRest("/data_to_compute_length");

    char* data_as_string = makeRestCall("/data_to_compute");

    int8_t* inputs = malloc(data_length);

    // split string into substrings
    char *dataToComputeSplit = strtok(data_as_string, ";");
    size_t index = 0;
    while (dataToComputeSplit != NULL) {
        inputs[index] = (int8_t)strtol(dataToComputeSplit, NULL, 10);
        dataToComputeSplit = strtok(NULL, ";");
    }
    return inputs;
}

static void getId(void) {
    uint8_t id[16] = {0};
    AI_getId(id);
    PRINT_BYTE_ARRAY("ID: ", id, 16);
}

typedef struct Command{
    char key;
    void (*fn)(void);
    char *name;
} Command;

typedef struct CommandList{
    uint8_t current_index;
    Command commands[20];
} CommandList;

void add_command(CommandList *self, Command command) {
    self->commands[self->current_index] = command;
    self->current_index++;
}


void list_commands(CommandList *self) {
    char text[256];
    for (int i = 0; i < self->current_index; i++) {
        Command c = self->commands[i];
        sprintf(text, "%c: %s", c.key, c.name);
        PRINT(text)
    }
}

void run_command(CommandList *self, char key) {
    int i;
    for (i = 0; i < self->current_index; i++) {
        Command c = self->commands[i];
        if (c.key == key) {
            PRINT("RUN COMMAND %s", c.name)
            c.fn();
            PRINT("DONE.")
            break;
        }
    }
    if (i == self->current_index) {
        PRINT("COMMAND NOT FOUND, AVAILABLE COMMANDS ARE")
        list_commands(self);
    }
}

CommandList commands;

#define ADD_COMMAND(_key, _fn) add_command(&commands, (Command){.fn=_fn, .key=_key, .name=#_fn})

void turnOffLeds(){
    middlewareSetFpgaLeds(0x00);
}

void turnOnLeds() {
    middlewareSetFpgaLeds(0xff);
}

void help() {
    list_commands(&commands);
}

void deploy() {
    uint8_t id[16] = {0};
    AI_deploy(1, id);
}

void predict() {
    int8_t* input_data = downloadDataToCompute();
    int result_length = getIntViaRest("/result_length");
    int8_t result[result_length];
    PRINT_BYTE_ARRAY("Predict for input: ", input_data, sizeof(input_data))
    AI_predict(input_data, sizeof(input_data), result, sizeof(result));
    free(input_data);
    PRINT_BYTE_ARRAY("Result: ", result, sizeof(result))
}

void middleware_read_from_addr_16() {
    uint8_t data[16] = {0};
    int8_t num_results = 16;

//    for(int i = 0; i < num_results; i++){
//        middlewareReadBlocking(16+i, data+i, 1);
//        middlewareReadBlocking(16+i, data+i, 1);
//    }
    middlewareReadBlocking(16, data, 16);

    PRINT_BYTE_ARRAY("Read data: ", data, 16)
}

_Noreturn static void runTest() {
    PRINT("===== START TEST =====")
    PRINT("Press 'h' to get all commands")
    ADD_COMMAND('P', env5HwFpgaPowersOn);
    ADD_COMMAND('p', env5HwFpgaPowersOff);
    ADD_COMMAND('g', getId);
    ADD_COMMAND('I', middlewareInit); // run after powering on fpga, needs to be rerun after powering fpga off
    ADD_COMMAND('i', middlewareDeinit);
    ADD_COMMAND('w', connectToWifi);
    ADD_COMMAND('u', middlewareUserlogicDisable);
    ADD_COMMAND('U', middlewareUserlogicEnable);
    ADD_COMMAND('f', downloadBinFile);
    ADD_COMMAND('l', turnOffLeds);
    ADD_COMMAND('L', turnOnLeds);
    ADD_COMMAND('h', help);
    ADD_COMMAND('d', deploy);
    ADD_COMMAND('x', predict);
    ADD_COMMAND('r', middleware_read_from_addr_16);
    while (1) {
        char c = getchar_timeout_us(UINT32_MAX);
        run_command(&commands, c);
    }
}

int main() {
    PRINT("Start")
    initHardware();
    PRINT("Hardware Init completed")
    runTest();
}
