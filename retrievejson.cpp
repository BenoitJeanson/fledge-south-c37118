/*
 * Fledge south plugin.

 * Copyright (c) 2022, RTE (http://www.rte-france.com)*

 * Released under the Apache 2.0 Licence
 *
 * Author: Benoit Jeanson <benoit.jeanson at rte-france.com>
 */

#include "retrievejson.h"

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, bool *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsBool())
    {
        return false;
    }
    *target = (*value)[key].GetBool();
    return true;
}

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, uint *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsUint())
    {
        return false;
    }
    *target = (*value)[key].GetUint();
    return true;
}

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, int *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsInt())
    {
        return false;
    }
    *target = (*value)[key].GetInt();
    return true;
}

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, std::string *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsString())
    {
        return false;
    }
    *target = (*value)[key].GetString();
    return true;
}

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, std::vector<std::string> *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsArray())
    {
        return false;
    }
    for (auto &val : (*value)[key].GetArray())
    {
        if (!val.IsString())
            return false;
        
        target->push_back(val.GetString());
    }
    return true;
}

bool RETRIEVEJSON::retrieve(rapidjson::Value *value, const char *key, std::vector<int> *target)
{
    if (!value->HasMember(key) || !(*value)[key].IsArray())
    {
        return false;
    }
    for (auto &val : (*value)[key].GetArray())
    {
        if (!val.IsInt())
            return false;
        
        target->push_back(val.GetInt());
    }
    return true;
}