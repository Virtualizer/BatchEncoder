﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <afxstr.h>
#include "ListT.h"
#include "Preset.h"

class CPresetsList : public CListT<CPreset>
{
public:
    void InsertNode(CString szName)
    {
        CPreset preset;
        preset.szName = szName;
        m_Items.AddTail(preset);
    }
};
