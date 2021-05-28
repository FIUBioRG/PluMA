// Copyright (C) 2020 FIU BioRG
// SPDX-License-Identifier: MIT

#ifndef _WRAPPER_H
#define _WRAPPER_H

extern void languageLoad(char* lang);
extern void languageUnload(char* lang);
extern void log(char* msg);
extern void dependency(char* plugin);
extern char* prefix();

#endif
