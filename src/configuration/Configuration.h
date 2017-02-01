﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "Options.h"
#include "FormatsList.h"
#include "ItemsList.h"
#include "LanguagesList.h"

class CConfiguration
{
public:
    COptions m_Options;
    CFormatsList m_Formats;
    CItemsList m_Items;
    CLanguagesList m_Languages;
    CLanguage* pLanguage;
public:
    CConfiguration()
    {
        this->pLanguage = NULL;
    }
    CConfiguration(const CConfiguration &other)
    {
        Copy(other);
    }
    CConfiguration& operator=(const CConfiguration &other)
    {
        Copy(other);
        return *this;
    }
    virtual ~CConfiguration()
    {
    }
public:
    void Copy(const CConfiguration &other)
    {
        this->m_Options = other.m_Options;
        this->m_Formats = other.m_Formats;
        this->m_Items = other.m_Items;
        this->m_Languages = other.m_Languages;
        this->pLanguage = NULL;
    }
public:
    CString GetString(const int nKey, const TCHAR* szDefault)
    {
        if (this->pLanguage != NULL)
        {
            CString rValue;
            if (this->pLanguage->m_Strings.m_Map.Lookup(nKey, rValue) == TRUE)
                return rValue;
        }
        return szDefault;
    }
};
