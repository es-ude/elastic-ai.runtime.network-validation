#include "RestMethods.h"
#include "AI.h"
#include "Common.h"
#include "Esp.h"
#include "Flash.h"
#include "FpgaConfigurationHandler.h"
#include "HTTP.h"
#include "Network.h"
#include "Sleep.h"
#include "Spi.h"
#include "enV5HwController.h"
#include "hardware/spi.h"
#include "middleware.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern char baseUrl[];


char* makeRestCall(const char* restCall){
    HttpResponse_t *response;
    char full_url[256] = {0};
    buildRestCallUrl(restCall, full_url, sizeof(full_url));
    HTTPGet(full_url, &response);
    char* out_string = malloc(response->length+1);
    memcpy(out_string, response->response, response->length);
    out_string[response->length] = '\0';
    HTTPCleanResponseBuffer(response);
    return out_string;
}

void buildRestCallUrl(const char* restCall, char* out, size_t out_size){
    memset(out, 0, out_size);
    strcpy(out, baseUrl);
    strcat(out, restCall);
}

int getIntViaRest(const char* restCall){
    char* length_as_string = makeRestCall(restCall);
    int data_length = strtol((char*)length_as_string, NULL, 10);
    free(length_as_string);
    return data_length;
}