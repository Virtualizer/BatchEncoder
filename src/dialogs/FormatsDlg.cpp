﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "StdAfx.h"
#include "..\BatchEncoder.h"
#include "..\utilities\Utilities.h"
#include "..\utilities\UnicodeUtf8.h"
#include "..\utilities\Utf8String.h"
#include "FormatsDlg.h"

#define FORMAT_COLUMN_NAME      0
#define FORMAT_COLUMN_TEMPLATE  1

IMPLEMENT_DYNAMIC(CFormatsDlg, CDialog)
CFormatsDlg::CFormatsDlg(CWnd* pParent /*=NULL*/)
    : CResizeDialog(CFormatsDlg::IDD, pParent)
{
    this->m_hIcon = AfxGetApp()->LoadIcon(IDI_TRAYICON);
    this->szFormatsDialogResize = _T("");
    this->szFormatsListColumns = _T("");
    this->bShowGridLines = true;
    this->bUpdate = false;
}

CFormatsDlg::~CFormatsDlg()
{

}

void CFormatsDlg::DoDataExchange(CDataExchange* pDX)
{
    CResizeDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC_GROUP_FORMAT_PIPES, m_GrpPipes);
    DDX_Control(pDX, IDC_STATIC_FORMAT_PATH, m_StcPath);
    DDX_Control(pDX, IDC_STATIC_FORMAT_TEMPLATE, m_StcTemplate);
    DDX_Control(pDX, IDC_STATIC_FORMAT_FUNCTIO, m_StcProgress);
    DDX_Control(pDX, IDC_LIST_FORMATS, m_LstFormats);
    DDX_Control(pDX, IDC_EDIT_FORMAT_PATH, m_EdtPath);
    DDX_Control(pDX, IDC_EDIT_FORMAT_TEMPLATE, m_EdtTemplate);
    DDX_Control(pDX, IDC_EDIT_FORMAT_FUNCTION, m_EdtFunction);
    DDX_Control(pDX, IDOK, m_BtnOK);
    DDX_Control(pDX, IDCANCEL, m_BtnCancel);
    DDX_Control(pDX, IDC_BUTTON_FORMAT_LOAD, m_BtnLoad);
    DDX_Control(pDX, IDC_BUTTON_FORMAT_SAVE, m_BtnSave);
    DDX_Control(pDX, IDC_BUTTON_BROWSE_PATH, m_BtnBrowse);
    DDX_Control(pDX, IDC_BUTTON_FORMAT_UPDATE, m_BtnChange);
}

BEGIN_MESSAGE_MAP(CFormatsDlg, CResizeDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FORMATS, OnLvnItemchangedListFormats)
    ON_BN_CLICKED(IDC_BUTTON_FORMAT_UPDATE, OnBnClickedButtonUpdateFormat)
    ON_BN_CLICKED(IDC_CHECK_FORMAT_PIPES_INPUT, OnBnClickedCheckPipesInput)
    ON_BN_CLICKED(IDC_CHECK_FORMAT_PIPES_OUTPUT, OnBnClickedCheckPipesOutput)
    ON_EN_CHANGE(IDC_EDIT_FORMAT_PATH, OnEnChangeEditFormatPath)
    ON_EN_CHANGE(IDC_EDIT_FORMAT_TEMPLATE, OnEnChangeEditFormatTemplate)
    ON_EN_CHANGE(IDC_EDIT_FORMAT_FUNCTION, OnEnChangeEditFormatFunction)
    ON_BN_CLICKED(IDC_BUTTON_FORMAT_LOAD, OnBnClickedButtonLoadFormats)
    ON_BN_CLICKED(IDC_BUTTON_FORMAT_SAVE, OnBnClickedButtonSaveFormats)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_PATH, OnBnClickedButtonBrowsePath)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE_PROGRESS, OnBnClickedButtonBrowseProgress)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CFormatsDlg::OnInitDialog()
{
    CResizeDialog::OnInitDialog();

    InitCommonControls();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // set pipes group font to normal
    m_GrpPipes.SetBold(false);

    // update list style
    DWORD dwExStyle = m_LstFormats.GetExtendedStyle();
    dwExStyle |= LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
    m_LstFormats.SetExtendedStyle(dwExStyle);

    // insert all ListCtrl columns
    m_LstFormats.InsertColumn(FORMAT_COLUMN_NAME, _T("Name"), LVCFMT_LEFT, 215);
    m_LstFormats.InsertColumn(FORMAT_COLUMN_TEMPLATE, _T("Template"), LVCFMT_LEFT, 360);

    // insert all ListCtrl items and sub items
    this->InsertFormatsToListCtrl();

    // setup resize anchors
    AddAnchor(IDC_LIST_FORMATS, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_STATIC_FORMAT_PATH, BOTTOM_LEFT);
    AddAnchor(IDC_EDIT_FORMAT_PATH, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON_BROWSE_PATH, BOTTOM_RIGHT);
    AddAnchor(IDC_STATIC_GROUP_FORMAT_PIPES, BOTTOM_LEFT);
    AddAnchor(IDC_CHECK_FORMAT_PIPES_INPUT, BOTTOM_LEFT);
    AddAnchor(IDC_CHECK_FORMAT_PIPES_OUTPUT, BOTTOM_LEFT);
    AddAnchor(IDC_STATIC_FORMAT_TEMPLATE, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT_FORMAT_TEMPLATE, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_STATIC_FORMAT_FUNCTIO, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT_FORMAT_FUNCTION, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON_BROWSE_PROGRESS, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON_FORMAT_LOAD, BOTTOM_LEFT);
    AddAnchor(IDC_BUTTON_FORMAT_SAVE, BOTTOM_LEFT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    this->LoadWindowSettings();

    if (this->bShowGridLines == true)
        this->ShowGridlines(true);

    return TRUE;
}

void CFormatsDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CResizeDialog::OnPaint();
    }
}

HCURSOR CFormatsDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CFormatsDlg::LoadWindowSettings()
{
    // set window rectangle and position
    if (szFormatsDialogResize.Compare(_T("")) != 0)
        this->SetWindowRectStr(szFormatsDialogResize);

    // load columns width for FormatsList
    if (szFormatsListColumns.Compare(_T("")) != 0)
    {
        int nColWidth[2];
        if (_stscanf(szFormatsListColumns, _T("%d %d"),
            &nColWidth[0],
            &nColWidth[1]) == 2)
        {
            for (int i = 0; i < 2; i++)
                m_LstFormats.SetColumnWidth(i, nColWidth[i]);
        }
    }
}

void CFormatsDlg::SaveWindowSettings()
{
    // save window rectangle and position
    this->szFormatsDialogResize = this->GetWindowRectStr();

    // save columns width from FormatsList
    int nColWidth[11];
    for (int i = 0; i < 2; i++)
        nColWidth[i] = m_LstFormats.GetColumnWidth(i);
    szFormatsListColumns.Format(_T("%d %d"),
        nColWidth[0],
        nColWidth[1]);
}

void CFormatsDlg::ShowGridlines(bool bShow)
{
    DWORD dwExStyle = m_LstFormats.GetExtendedStyle();
    if (bShow == true)
    {
        dwExStyle |= LVS_EX_GRIDLINES;
        m_LstFormats.SetExtendedStyle(dwExStyle);
    }
    else
    {
        if (dwExStyle & LVS_EX_GRIDLINES)
        {
            dwExStyle = dwExStyle ^ LVS_EX_GRIDLINES;
            m_LstFormats.SetExtendedStyle(dwExStyle);
        }
    }
}

void CFormatsDlg::AddToList(CFormat &format, int nItem)
{
    LVITEM lvi;

    ZeroMemory(&lvi, sizeof(LVITEM));

    lvi.mask = LVIF_TEXT | LVIF_STATE;
    lvi.state = 0;
    lvi.stateMask = 0;
    lvi.iItem = nItem;

    lvi.iSubItem = FORMAT_COLUMN_NAME;
    lvi.pszText = (LPTSTR)(LPCTSTR)(format.szName);
    m_LstFormats.InsertItem(&lvi);

    lvi.iSubItem = FORMAT_COLUMN_TEMPLATE;
    lvi.pszText = (LPTSTR)(LPCTSTR)(format.szTemplate);
    m_LstFormats.SetItemText(lvi.iItem, FORMAT_COLUMN_TEMPLATE, lvi.pszText);
}

void CFormatsDlg::InsertFormatsToListCtrl()
{
    int nFormats = m_Formats.GetSize();
    for (int i = 0; i < nFormats; i++)
    {
        CFormat& format = m_Formats.GetData(i);
        this->AddToList(format, i);
    }
}

void CFormatsDlg::ListSelectionChange()
{
    if (bUpdate == true)
        return;

    bUpdate = true;


    POSITION pos = m_LstFormats.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_LstFormats.GetNextSelectedItem(pos);

        CFormat& format = this->m_Formats.GetData(nItem);

        this->m_EdtTemplate.SetWindowText(format.szTemplate);
        this->m_EdtPath.SetWindowText(format.szPath);

        if (format.bInput)
            CheckDlgButton(IDC_CHECK_FORMAT_PIPES_INPUT, BST_CHECKED);
        else
            CheckDlgButton(IDC_CHECK_FORMAT_PIPES_INPUT, BST_UNCHECKED);

        if (format.bOutput)
            CheckDlgButton(IDC_CHECK_FORMAT_PIPES_OUTPUT, BST_CHECKED);
        else
            CheckDlgButton(IDC_CHECK_FORMAT_PIPES_OUTPUT, BST_UNCHECKED);

        this->m_EdtFunction.SetWindowText(format.szFunction);
    }
    else
    {
        this->m_EdtTemplate.SetWindowText(_T(""));
        this->m_EdtPath.SetWindowText(_T(""));
        CheckDlgButton(IDC_CHECK_FORMAT_PIPES_INPUT, BST_UNCHECKED);
        CheckDlgButton(IDC_CHECK_FORMAT_PIPES_OUTPUT, BST_UNCHECKED);
        this->m_EdtFunction.SetWindowText(_T(""));
    }

    bUpdate = false;
}

void CFormatsDlg::LoadFormats(CString szFileXml)
{
    XmlConfiguration doc;
    if (doc.Open(szFileXml) == true)
    {
        this->m_Formats.RemoveAllNodes();
        this->m_LstFormats.DeleteAllItems();

        doc.GetFormats(this->m_Formats);

        this->InsertFormatsToListCtrl();
        this->ListSelectionChange();
    }
    else
    {
        MessageBox(_T("Failed to load file!"), _T("ERROR"), MB_OK | MB_ICONERROR);
    }
}

void CFormatsDlg::SaveFormats(CString szFileXml)
{
    XmlConfiguration doc;
    doc.SetFormats(this->m_Formats);
    if (doc.Save(szFileXml) != true)
    {
        MessageBox(_T("Failed to save file!"), _T("ERROR"), MB_OK | MB_ICONERROR);
    }
}

bool CFormatsDlg::BrowseForPath(CString szDefaultFName, CEdit *pEdit, int nID)
{
    CFileDialog fd(TRUE, _T("exe"), szDefaultFName,
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER,
        _T("Exe Files (*.exe)|*.exe|All Files|*.*||"), this);

    if (fd.DoModal() == IDOK)
    {
        CString szPath = fd.GetPathName();
        pEdit->SetWindowText(szPath);
        return true;
    }
    return false;
}

bool CFormatsDlg::BrowseForFunction(CString szDefaultFName, CEdit *pEdit, int nID)
{
    CFileDialog fd(TRUE, _T("progress"), szDefaultFName,
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER,
        _T("Progress Files (*.progress)|*.progress|All Files|*.*||"), this);

    if (fd.DoModal() == IDOK)
    {
        CString szPath;
        szPath = fd.GetPathName();
        pEdit->SetWindowText(szPath);
        return true;
    }
    return false;
}

void CFormatsDlg::OnBnClickedOk()
{
    this->SaveWindowSettings();

    OnOK();
}

void CFormatsDlg::OnBnClickedCancel()
{
    this->SaveWindowSettings();

    OnCancel();
}

void CFormatsDlg::OnLvnItemchangedListFormats(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    this->ListSelectionChange();

    *pResult = 0;
}

void CFormatsDlg::OnBnClickedButtonUpdateFormat()
{
    if (bUpdate == true)
        return;

    bUpdate = true;

    POSITION pos = m_LstFormats.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_LstFormats.GetNextSelectedItem(pos);

        //CString szName = _T("");
        CString szTemplate = _T("");
        CString szPath = _T("");
        bool bInput = false;
        bool bOutput = false;
        CString szFunction = _T("");

        //this->m_EdtName.GetWindowText(szName);
        this->m_EdtTemplate.GetWindowText(szTemplate);

        if (IsDlgButtonChecked(IDC_CHECK_FORMAT_PIPES_INPUT) == BST_CHECKED)
            bInput = true;

        if (IsDlgButtonChecked(IDC_CHECK_FORMAT_PIPES_OUTPUT) == BST_CHECKED)
            bOutput = true;

        this->m_EdtPath.GetWindowText(szPath);
        this->m_EdtFunction.GetWindowText(szFunction);

        CFormat& format = m_Formats.GetData(nItem);
        format.szTemplate = szTemplate;
        format.bInput = bInput;
        format.bOutput = bOutput;
        format.szPath = szPath;
        format.szFunction = szFunction;

        //m_LstFormats.SetItemText(nItem, FORMAT_COLUMN_NAME, szName);
        m_LstFormats.SetItemText(nItem, FORMAT_COLUMN_TEMPLATE, szTemplate);

        m_LstFormats.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);
    }

    bUpdate = false;
}

void CFormatsDlg::OnBnClickedCheckPipesInput()
{
    if (bUpdate == true)
        return;

    OnBnClickedButtonUpdateFormat();
}

void CFormatsDlg::OnBnClickedCheckPipesOutput()
{
    if (bUpdate == true)
        return;

    OnBnClickedButtonUpdateFormat();
}

void CFormatsDlg::OnEnChangeEditFormatPath()
{
    if (bUpdate == true)
        return;

    OnBnClickedButtonUpdateFormat();
}

void CFormatsDlg::OnEnChangeEditFormatTemplate()
{
    if (bUpdate == true)
        return;

    OnBnClickedButtonUpdateFormat();
}

void CFormatsDlg::OnEnChangeEditFormatFunction()
{
    if (bUpdate == true)
        return;

    OnBnClickedButtonUpdateFormat();
}

void CFormatsDlg::OnBnClickedButtonLoadFormats()
{
    CFileDialog fd(TRUE, _T("formats"), _T(""),
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER,
        _T("Formats Files (*.formats)|*.formats|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    if (fd.DoModal() == IDOK)
    {
        CString szFileXml = fd.GetPathName();
        this->LoadFormats(szFileXml);
    }
}

void CFormatsDlg::OnBnClickedButtonSaveFormats()
{
    CFileDialog fd(FALSE, _T("formats"), _T("formats"),
        OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_EXPLORER | OFN_OVERWRITEPROMPT,
        _T("Formats Files (*.formats)|*.formats|Xml Files (*.xml)|*.xml|All Files|*.*||"), this);

    if (fd.DoModal() == IDOK)
    {
        CString szFileXml = fd.GetPathName();
        this->SaveFormats(szFileXml);
    }
}

void CFormatsDlg::OnBnClickedButtonBrowsePath()
{
    POSITION pos = m_LstFormats.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_LstFormats.GetNextSelectedItem(pos);
        CString szPath;
        this->m_EdtPath.GetWindowText(szPath);
        BrowseForPath(szPath, &m_EdtPath, nItem);
    }
    else
    {
        MessageBox(_T("Please select item in the list."),
            _T("INFO"),
            MB_OK | MB_ICONINFORMATION);
    }
}

void CFormatsDlg::OnBnClickedButtonBrowseProgress()
{
    POSITION pos = m_LstFormats.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_LstFormats.GetNextSelectedItem(pos);
        CString szFunction;
        this->m_EdtFunction.GetWindowText(szFunction);
        BrowseForFunction(szFunction, &m_EdtFunction, nItem);
    }
    else
    {
        MessageBox(_T("Please select item in the list."),
            _T("INFO"),
            MB_OK | MB_ICONINFORMATION);
    }
}

void CFormatsDlg::OnClose()
{
    this->SaveWindowSettings();

    CResizeDialog::OnClose();
}
