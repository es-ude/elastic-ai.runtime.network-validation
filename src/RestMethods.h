#ifndef ENV5_NETWORKVALIDATION_RESTMETHODS_H
#define ENV5_NETWORKVALIDATION_RESTMETHODS_H

#include <stddef.h>

int getIntViaRest(const char* rest_method);
char* makeRestCall(const char* restCall);
void buildRestCallUrl(const char* restCall, char* out, size_t out_size);

#endif // ENV5_NETWORKVALIDATION_RESTMETHODS_H
