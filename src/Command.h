#ifndef ENV5_NETWORKVALIDATION_COMMAND_H
#define ENV5_NETWORKVALIDATION_COMMAND_H

#include <stdint.h>

#define ADD_COMMAND(_key, _fn) add_command(&commands, (Command){.fn=_fn, .key=_key, .name=#_fn})
#define MAX_NUMBER_OF_COMMANDS 20

typedef struct Command{
    char key;
    void (*fn)(void);
    char *name;
} Command;


typedef struct CommandList{
    uint8_t current_index;
    Command commands[MAX_NUMBER_OF_COMMANDS];
} CommandList;


void run_command(CommandList *self, char key);
void list_commands(CommandList *self);
void add_command(struct CommandList *self, struct Command command);

#endif // ENV5_NETWORKVALIDATION_COMMAND_H
