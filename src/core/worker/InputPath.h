﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>

namespace worker
{
    class CInputPath
    {
    private:
        wchar_t szInputFile[_MAX_PATH];
        wchar_t szInputFilePath[_MAX_PATH];
    public:
        wchar_t szInputDrive[_MAX_DRIVE];
        wchar_t szInputDir[_MAX_DIR];
        wchar_t szInputName[_MAX_FNAME];
        wchar_t szInputExt[_MAX_EXT];
        wchar_t szInputPath[_MAX_PATH];
        std::vector<std::wstring> szSourceFolders;
    public:
        CInputPath(const wchar_t szInputFile[])
        {
            wcscpy_s(this->szInputFile, szInputFile);

            // make full path
            _wfullpath(this->szInputFilePath, this->szInputFile, _MAX_PATH);

            // split full path
            _wsplitpath_s(this->szInputFilePath, this->szInputDrive, this->szInputDir, this->szInputName, this->szInputExt);

            // source directory
            _wmakepath_s(this->szInputPath, this->szInputDrive, this->szInputDir, nullptr, nullptr);

            // find dir folder names
            wchar_t szSourceDirCopy[_MAX_DIR];
            wcscpy_s(szSourceDirCopy, this->szInputDir);

            wchar_t szSeparators[] = L"\\/";
            wchar_t *szNextToken = nullptr;
            wchar_t *szToken = wcstok_s(szSourceDirCopy, szSeparators, &szNextToken);
            if (szToken != nullptr)
                this->szSourceFolders.emplace_back(szToken);

            while (szToken != nullptr)
            {
                szToken = wcstok_s(nullptr, szSeparators, &szNextToken);
                if (szToken != nullptr)
                    this->szSourceFolders.emplace_back(szToken);
            }
        }
    };
}
