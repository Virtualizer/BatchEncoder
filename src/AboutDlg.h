﻿//
// BatchEncoder (Audio Conversion GUI)
// Copyright (C) 2005-2017 Wiesław Šoltés <wieslaw.soltes@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "afxcmn.h"
#include "afxwin.h"

#include "MyStatic.h"
#include "MyButton.h"
#include "Hyperlink.h"

class CAboutDlg : public CDialog
{
    DECLARE_DYNAMIC(CAboutDlg)
public:
    CAboutDlg(CWnd* pParent = NULL);
    virtual ~CAboutDlg();
    enum { IDD = IDD_DIALOG_ABOUT };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
protected:
    virtual void OnOK();
    virtual void OnCancel();
public:
    CMyButton m_BtnOK; 
public:
    CMyStatic m_StcMainAppName;
    CHyperlink m_StcWebsite;
    CHyperlink m_StcEmail;
    CMyStatic m_StcLicense;
public:
    afx_msg void OnClose();
public:
    afx_msg void OnBnClickedOk();
};
