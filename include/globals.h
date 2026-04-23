#ifndef GLOBALS_H
#define GLOBALS_H

#include "executor.h"
#include "parser.h"
#include "command.h"

extern std::unique_ptr<Executor> executor;
extern std::unique_ptr<Parser> parser;

#endif