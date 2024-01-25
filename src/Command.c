#include "Command.h"
#include "Common.h"
#include <stdint-gcc.h>
#include <stdio.h>



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


void add_command(struct CommandList *self, struct Command command) {
    self->commands[self->current_index] = command;
    self->current_index++;
}