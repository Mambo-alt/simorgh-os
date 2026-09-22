#ifndef SHELL_H
#define SHELL_H

#include "keyboard.h"

void shell_init(void);
void shell_execute(char *line);
void shell_prompt(void);
void shell_handle_event(key_event_t *ev);

#endif
