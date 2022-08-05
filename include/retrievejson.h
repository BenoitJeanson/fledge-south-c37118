/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#ifndef RETRIEVEJSON_H
#define RETRIEVEJSON_H

#include "rapidjson/document.h"
#include "logger.h"
#include <vector>

class RETRIEVEJSON
{
public:
    static bool retrieve(rapidjson::Value *doc, const char *key, bool *target);
    static bool retrieve(rapidjson::Value *doc, const char *key, uint *target);
    static bool retrieve(rapidjson::Value *doc, const char *key, int *target);
    static bool retrieve(rapidjson::Value *doc, const char *key, std::string *target);
    static bool retrieve(rapidjson::Value *doc, const char *key, std::vector<std::string> *target);
    static bool retrieve(rapidjson::Value *doc, const char *key, std::vector<int> *target);
};

#endif
