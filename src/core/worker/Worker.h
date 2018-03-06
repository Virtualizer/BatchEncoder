﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <queue>
#include <memory>
#include <utility>
#include <string>
#include <mutex>
#include <thread>
#include "utilities\Log.h"
#include "utilities\MemoryLog.h"
#include "utilities\Pipe.h"
#include "utilities\Process.h"
#include "utilities\String.h"
#include "utilities\TimeCount.h"
#include "utilities\Utilities.h"
#include "WorkerContext.h"
#include "CommandLine.h"
#include "OutputPath.h"
#include "FileToPipeWriter.h"
#include "PipeToFileWriter.h"
#include "PipeToStringWriter.h"
#include "OutputParser.h"
#include "LuaOutputParser.h"
#include "DebugOutputParser.h"
#include "ToolUtilities.h"

namespace worker
{
    class CWorker
    {
    public:
        bool CWorker::ConvertFileUsingConsole(IWorkerContext* ctx, CCommandLine &cl, std::mutex &m_down)
        {
            auto config = ctx->pConfig;
            util::CProcess process;
            util::CPipe Stderr(true);
            util::CTimeCount timer;
            CLuaOutputParser parser;
            CPipeToStringWriter writer;

            if ((cl.bUseReadPipes == true) || (cl.bUseWritePipes == true))
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00120001));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }

            // create pipes for stderr
            if (Stderr.Create() == false)
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00120002));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }

            // duplicate stderr read pipe handle to prevent child process from closing the pipe
            if (Stderr.DuplicateRead() == false)
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00120003));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }

            // connect pipes to process
            process.ConnectStdInput(nullptr);
            process.ConnectStdOutput(Stderr.hWrite);
            process.ConnectStdError(Stderr.hWrite);

            m_down.lock();
            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

            timer.Start();
            if (process.Start(cl.szCommandLine, config->m_Options.bHideConsoleWindow) == false)
            {
                bool bFailed = true;

                if (config->m_Options.bTryToDownloadTools == true)
                {
                    CToolUtilities m_Utilities;

                    int nTool = config::CTool::GetToolByPath(config->m_Tools, cl.format->szPath);
                    if (nTool < 0)
                    {
                        nTool = m_Utilities.FindTool(config->m_Tools, cl.format->szId);
                    }

                    if (nTool >= 0)
                    {
                        config::CTool& tool = config->m_Tools[nTool];
                        bool bResult = m_Utilities.Download(tool, true, true, nTool, config,
                            [this, ctx, &cl](int nIndex, std::wstring szStatus) -> bool
                        {
                            ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), szStatus);
                            if (ctx->bRunning == false)
                                return true;
                            return false;
                        });

                        if (bResult == true)
                        {
                            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

                            if (process.Start(cl.szCommandLine, config->m_Options.bHideConsoleWindow) == true)
                            {
                                bFailed = false;
                            }
                        }
                    }
                }

                if (bFailed == true)
                {
                    m_down.unlock();

                    timer.Stop();

                    Stderr.CloseRead();
                    Stderr.CloseWrite();

                    std::wstring szStatus = ctx->GetString(0x00120004) + L" (" + std::to_wstring(::GetLastError()) + L")";
                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), szStatus);
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }
            }

            m_down.unlock();

            // close unused pipe handle
            Stderr.CloseWrite();

            //parser.Log = std::make_unique<util::MemoryLog>();

            // console progress loop
            if (writer.ReadLoop(ctx, cl, Stderr, parser, m_down) == false)
            {
                timer.Stop();
                Stderr.CloseRead();
                process.Stop(false, cl.format->nExitCodeSuccess);
                return false;
            }

            timer.Stop();
            Stderr.CloseRead();
            if (process.Stop(parser.nProgress == 100, cl.format->nExitCodeSuccess) == false)
                parser.nProgress = -1;

            if (parser.nProgress != 100)
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00120005));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }
            else
            {
                ctx->ItemStatus(cl.nItemId, util::CTimeCount::Format(timer.ElapsedTime()), ctx->GetString(0x00120006));
                ctx->ItemProgress(cl.nItemId, 100, true, false);
                return true;
            }
        }
        bool CWorker::ConvertFileUsingPipes(IWorkerContext* ctx, CCommandLine &cl, std::mutex &m_down)
        {
            auto config = ctx->pConfig;
            util::CProcess process;
            util::CPipe Stdin(true);
            util::CPipe Stdout(true);
            CFileToPipeWriter readContext;
            CPipeToFileWriter writeContext;
            std::thread readThread;
            std::thread writeThread;
            int nProgress = 0;
            util::CTimeCount timer;

            if ((cl.bUseReadPipes == false) && (cl.bUseWritePipes == false))
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130001));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }

            if (cl.bUseReadPipes == true)
            {
                // create pipes for stdin
                if (Stdin.Create() == false)
                {
                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130002));
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }

                // set stdin write pipe inherit flag
                if (Stdin.InheritWrite() == false)
                {
                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130003));
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }
            }

            if (cl.bUseWritePipes == true)
            {
                // create pipes for stdout
                if (Stdout.Create() == false)
                {
                    if (cl.bUseReadPipes == true)
                    {
                        Stdin.CloseRead();
                        Stdin.CloseWrite();
                    }

                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130004));
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }

                // set stdout read pipe inherit flag
                if (Stdout.InheritRead() == false)
                {
                    if (cl.bUseReadPipes == true)
                    {
                        Stdin.CloseRead();
                        Stdin.CloseWrite();
                    }

                    Stdout.CloseRead();
                    Stdout.CloseWrite();

                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130005));
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }
            }

            // connect pipes to process
            if ((cl.bUseReadPipes == true) && (cl.bUseWritePipes == false))
            {
                process.ConnectStdInput(Stdin.hRead);
                process.ConnectStdOutput(GetStdHandle(STD_OUTPUT_HANDLE));
                process.ConnectStdError(GetStdHandle(STD_ERROR_HANDLE));
            }
            else if ((cl.bUseReadPipes == false) && (cl.bUseWritePipes == true))
            {
                process.ConnectStdInput(GetStdHandle(STD_INPUT_HANDLE));
                process.ConnectStdOutput(Stdout.hWrite);
                process.ConnectStdError(GetStdHandle(STD_ERROR_HANDLE));
            }
            else if ((cl.bUseReadPipes == true) && (cl.bUseWritePipes == true))
            {
                process.ConnectStdInput(Stdin.hRead);
                process.ConnectStdOutput(Stdout.hWrite);
                process.ConnectStdError(GetStdHandle(STD_ERROR_HANDLE));
            }

            m_down.lock();
            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

            timer.Start();
            if (process.Start(cl.szCommandLine, config->m_Options.bHideConsoleWindow) == false)
            {
                bool bFailed = true;

                if (config->m_Options.bTryToDownloadTools == true)
                {
                    CToolUtilities m_Utilities;

                    int nTool = config::CTool::GetToolByPath(config->m_Tools, cl.format->szPath);
                    if (nTool < 0)
                    {
                        nTool = m_Utilities.FindTool(config->m_Tools, cl.format->szId);
                    }

                    if (nTool >= 0)
                    {
                        config::CTool& tool = config->m_Tools[nTool];
                        bool bResult = m_Utilities.Download(tool, true, true, nTool, config,
                            [this, ctx, &cl](int nIndex, std::wstring szStatus) -> bool
                        {
                            ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), szStatus);
                            if (ctx->bRunning == false)
                                return true;
                            return false;
                        });

                        if (bResult == true)
                        {
                            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

                            if (process.Start(cl.szCommandLine, config->m_Options.bHideConsoleWindow) == true)
                            {
                                bFailed = false;
                            }
                        }
                    }
                }

                if (bFailed == true)
                {
                    m_down.unlock();

                    timer.Stop();

                    if (cl.bUseReadPipes == true)
                    {
                        Stdin.CloseRead();
                        Stdin.CloseWrite();
                    }

                    if (cl.bUseWritePipes == true)
                    {
                        Stdout.CloseRead();
                        Stdout.CloseWrite();
                    }

                    std::wstring szStatus = ctx->GetString(0x00130006) + L" (" + std::to_wstring(::GetLastError()) + L")";
                    ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), szStatus);
                    ctx->ItemProgress(cl.nItemId, -1, true, true);
                    return false;
                }
            }

            m_down.unlock();

            // close unused pipe handles
            if (cl.bUseReadPipes == true)
                Stdin.CloseRead();

            if (cl.bUseWritePipes == true)
                Stdout.CloseWrite();

            // create read thread
            if (cl.bUseReadPipes == true)
            {
                readContext.bError = false;
                readContext.bFinished = false;
                readContext.szFileName = cl.szInputFile;
                readContext.nIndex = cl.nItemId;

                readThread = std::thread([this, ctx, &readContext, &Stdin]() { readContext.ReadLoop(ctx, Stdin); });

                // wait for read thread to finish
                if (cl.bUseWritePipes == false)
                {
                    readThread.join();

                    // NOTE: Handle is closed in ReadThread.
                    //Stdin.CloseWrite();

                    // check for result from read thread
                    if ((readContext.bError == false) && (readContext.bFinished == true))
                        nProgress = 100;
                    else
                        nProgress = -1;
                }
            }

            // create write thread
            if (cl.bUseWritePipes == true)
            {
                writeContext.bError = false;
                writeContext.bFinished = false;
                writeContext.szFileName = cl.szOutputFile;
                writeContext.nIndex = cl.nItemId;

                writeThread = std::thread([this, ctx, &writeContext, &Stdout]() { writeContext.WriteLoop(ctx, Stdout); });

                if (cl.bUseReadPipes == true)
                {
                    // wait for read thread to finish
                    readThread.join();

                    // NOTE: Handle is closed in ReadThread.
                    //Stdin.CloseWrite();

                    if ((readContext.bError == true) || (readContext.bFinished == false))
                    {
                        // close write thread handle
                        Stdout.CloseRead();

                        // read thread failed so terminate write thread
                        writeThread.join();
                    }
                    else
                    {
                        // wait for write thread to finish
                        writeThread.join();

                        // close write thread handle
                        Stdout.CloseRead();
                    }

                    // check for result from read thread
                    if ((readContext.bError == false) && (readContext.bFinished == true)
                        && (writeContext.bError == false) && (writeContext.bFinished == true))
                        nProgress = 100;
                    else
                        nProgress = -1;
                }
                else
                {
                    // wait for write thread to finish
                    writeThread.join();

                    // close write thread handle
                    Stdout.CloseRead();

                    // check for result from write thread
                    if ((writeContext.bError == false) && (writeContext.bFinished == true))
                        nProgress = 100;
                    else
                        nProgress = -1;
                }
            }

            timer.Stop();

            if (process.Stop(nProgress == 100, cl.format->nExitCodeSuccess) == false)
                nProgress = -1;

            if (nProgress != 100)
            {
                ctx->ItemStatus(cl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130009));
                ctx->ItemProgress(cl.nItemId, -1, true, true);
                return false;
            }
            else
            {
                ctx->ItemStatus(cl.nItemId, util::CTimeCount::Format(timer.ElapsedTime()), ctx->GetString(0x0013000B));
                ctx->ItemProgress(cl.nItemId, 100, true, false);
                return true;
            }
        }
        bool CWorker::ConvertFileUsingOnlyPipes(IWorkerContext* ctx, CCommandLine &dcl, CCommandLine &ecl, std::mutex &m_down)
        {
            auto config = ctx->pConfig;
            util::CProcess decoderProcess;
            util::CProcess encoderProcess;
            util::CPipe Stdin(true);
            util::CPipe Stdout(true);
            util::CPipe Bridge(true);
            CFileToPipeWriter readContext;
            CPipeToFileWriter writeContext;
            std::thread readThread;
            std::thread writeThread;
            int nProgress = 0;
            util::CTimeCount timer;

            // create pipes for stdin
            if (Stdin.Create() == false)
            {
                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130002));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }

            // set stdin write pipe inherit flag
            if (Stdin.InheritWrite() == false)
            {
                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130003));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }

            // create pipes for stdout
            if (Stdout.Create() == false)
            {
                Stdin.CloseRead();
                Stdin.CloseWrite();

                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130004));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }

            // set stdout read pipe inherit flag
            if (Stdout.InheritRead() == false)
            {
                Stdin.CloseRead();
                Stdin.CloseWrite();

                Stdout.CloseRead();
                Stdout.CloseWrite();

                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130005));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }

            // create pipes for processes bridge
            if (Bridge.Create() == false)
            {
                Stdin.CloseRead();
                Stdin.CloseWrite();

                Stdout.CloseRead();
                Stdout.CloseWrite();

                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x0013000A));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }

            // connect pipes to decoder process
            decoderProcess.ConnectStdInput(Stdin.hRead);
            decoderProcess.ConnectStdOutput(Bridge.hWrite);
            decoderProcess.ConnectStdError(GetStdHandle(STD_ERROR_HANDLE));

            // connect pipes to encoder process
            encoderProcess.ConnectStdInput(Bridge.hRead);
            encoderProcess.ConnectStdOutput(Stdout.hWrite);
            encoderProcess.ConnectStdError(GetStdHandle(STD_ERROR_HANDLE));

            timer.Start();

            m_down.lock();
            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

            // create decoder process
            if (decoderProcess.Start(dcl.szCommandLine, config->m_Options.bHideConsoleWindow) == false)
            {
                bool bFailed = true;

                if (config->m_Options.bTryToDownloadTools == true)
                {
                    CToolUtilities m_Utilities;

                    int nTool = config::CTool::GetToolByPath(config->m_Tools, dcl.format->szPath);
                    if (nTool < 0)
                    {
                        nTool = m_Utilities.FindTool(config->m_Tools, dcl.format->szId);
                    }

                    if (nTool >= 0)
                    {
                        config::CTool& tool = config->m_Tools[nTool];
                        bool bResult = m_Utilities.Download(tool, true, true, nTool, config,
                            [this, ctx, &dcl](int nIndex, std::wstring szStatus) -> bool
                        {
                            ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), szStatus);
                            if (ctx->bRunning == false)
                                return true;
                            return false;
                        });

                        if (bResult == true)
                        {
                            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

                            if (decoderProcess.Start(dcl.szCommandLine, config->m_Options.bHideConsoleWindow) == true)
                            {
                                bFailed = false;
                            }
                        }
                    }
                }

                if (bFailed == true)
                {
                    m_down.unlock();

                    timer.Stop();

                    Stdin.CloseRead();
                    Stdin.CloseWrite();

                    Stdout.CloseRead();
                    Stdout.CloseWrite();

                    Bridge.CloseRead();
                    Bridge.CloseWrite();

                    std::wstring szStatus = ctx->GetString(0x00130006) + L" (" + std::to_wstring(::GetLastError()) + L")";
                    ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), szStatus);
                    ctx->ItemProgress(dcl.nItemId, -1, true, true);
                    return false;
                }
            }

            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

            // create encoder process
            if (encoderProcess.Start(ecl.szCommandLine, config->m_Options.bHideConsoleWindow) == false)
            {
                bool bFailed = true;

                if (config->m_Options.bTryToDownloadTools == true)
                {
                    CToolUtilities m_Utilities;

                    int nTool = -1;
                    nTool = config::CTool::GetToolByPath(config->m_Tools, ecl.format->szPath);
                    if (nTool < 0)
                    {
                        nTool = m_Utilities.FindTool(config->m_Tools, ecl.format->szId);
                    }

                    if (nTool >= 0)
                    {
                        config::CTool& tool = config->m_Tools[nTool];
                        bool bResult = m_Utilities.Download(tool, true, true, nTool, config,
                            [this, ctx, &ecl](int nIndex, std::wstring szStatus) -> bool
                        {
                            ctx->ItemStatus(ecl.nItemId, ctx->GetString(0x00150001), szStatus);
                            if (ctx->bRunning == false)
                                return true;
                            return false;
                        });

                        if (bResult == true)
                        {
                            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

                            if (encoderProcess.Start(ecl.szCommandLine, config->m_Options.bHideConsoleWindow) == true)
                            {
                                bFailed = false;
                            }
                        }
                    }
                }

                if (bFailed == true)
                {
                    m_down.unlock();

                    timer.Stop();

                    decoderProcess.Stop(false, dcl.format->nExitCodeSuccess);

                    Stdin.CloseRead();
                    Stdin.CloseWrite();

                    Stdout.CloseRead();
                    Stdout.CloseWrite();

                    Bridge.CloseRead();
                    Bridge.CloseWrite();

                    std::wstring szStatus = ctx->GetString(0x00130006) + L" (" + std::to_wstring(::GetLastError()) + L")";
                    ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), szStatus);
                    ctx->ItemProgress(dcl.nItemId, -1, true, true);
                    return false;
                }
            }

            m_down.unlock();

            // close unused pipe handles
            Stdin.CloseRead();
            Stdout.CloseWrite();
            Bridge.CloseWrite();
            Bridge.CloseRead();

            // create read thread
            readContext.bError = false;
            readContext.bFinished = false;
            readContext.szFileName = dcl.szInputFile;
            readContext.nIndex = dcl.nItemId;

            readThread = std::thread([this, ctx, &readContext, &Stdin]() { readContext.ReadLoop(ctx, Stdin); });

            // create write thread
            writeContext.bError = false;
            writeContext.bFinished = false;
            writeContext.szFileName = ecl.szOutputFile;
            writeContext.nIndex = ecl.nItemId;

            writeThread = std::thread([this, ctx, &writeContext, &Stdout]() { writeContext.WriteLoop(ctx, Stdout); });

            // wait for read thread to finish after write thread finished
            readThread.join();

            // NOTE: Handle is closed in ReadThread.
            //Stdin.CloseWrite();

            if ((readContext.bError == true) || (readContext.bFinished == false))
            {
                Stdout.CloseRead();

                // read thread failed so terminate write thread
                writeThread.join();
            }
            else
            {
                Stdout.CloseRead();

                // wait for write thread to finish
                writeThread.join();
            }

            // check for result from read and write thread
            if ((readContext.bError == false) && (readContext.bFinished == true)
                && (writeContext.bError == false) && (writeContext.bFinished == true))
                nProgress = 100;
            else
                nProgress = -1;

            timer.Stop();

            if (decoderProcess.Stop(nProgress == 100, dcl.format->nExitCodeSuccess) == false)
                nProgress = -1;

            if (encoderProcess.Stop(nProgress == 100, ecl.format->nExitCodeSuccess) == false)
                nProgress = -1;

            if (nProgress != 100)
            {
                ctx->ItemStatus(dcl.nItemId, ctx->GetString(0x00150001), ctx->GetString(0x00130009));
                ctx->ItemProgress(dcl.nItemId, -1, true, true);
                return false;
            }
            else
            {
                ctx->ItemStatus(dcl.nItemId, util::CTimeCount::Format(timer.ElapsedTime()), ctx->GetString(0x0013000B));
                ctx->ItemProgress(dcl.nItemId, 100, true, false);
                return true;
            }
        }
        bool CWorker::ConvertItem(IWorkerContext* ctx, config::CItem& item, std::mutex &m_dir, std::mutex &m_down)
        {
            auto config = ctx->pConfig;
            config::CFormat *ef = nullptr;
            config::CFormat *df = nullptr;
            std::wstring szEncInputFile;
            std::wstring szEncOutputFile;
            std::wstring szDecInputFile;
            std::wstring szDecOutputFile;
            COutputPath m_Output;
            CCommandLine dcl;
            CCommandLine ecl;

            // prepare encoder
            config::CPath& path = item.m_Paths[0];

            szEncInputFile = path.szPath;
            if (util::Utilities::FileExists(szEncInputFile) == false)
            {
                ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140001));
                return false;
            }

            int nEncoder = config::CFormat::GetFormatById(config->m_Formats, item.szFormatId);
            if (nEncoder == -1)
            {
                ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140002));
                return false;
            }

            ef = &config->m_Formats[nEncoder];

            if (item.nPreset >= ef->m_Presets.size())
            {
                ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140003));
                return false;
            }

            bool bCanEncode = ef->IsValidInputExtension(util::Utilities::GetFileExtension(szEncInputFile));

            szEncOutputFile = m_Output.CreateFilePath(config->m_Options.szOutputPath, szEncInputFile, item.szName, ef->szOutputExtension);

            if (config->m_Options.bOverwriteExistingFiles == false)
            {
                if (util::Utilities::FileExists(szEncOutputFile) == true)
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140010));
                    return false;
                }
            }

            // create output path
            m_dir.lock();

            if (m_Output.CreateOutputPath(szEncOutputFile) == false)
            {
                m_dir.unlock();
                ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000F));
                return false;
            }

            m_dir.unlock();

            util::Utilities::SetCurrentDirectory(config->m_Settings.szSettingsPath);

            // prepare decoder
            if (bCanEncode == false)
            {
                int nDecoder = config::CFormat::GetDecoderByExtensionAndFormat(config->m_Formats, item.szExtension, ef);
                if (nDecoder == -1)
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140004));
                    return false;
                }

                df = &config->m_Formats[nDecoder];

                if (df->nDefaultPreset >= df->m_Presets.size())
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140005));
                    return false;
                }

                bool bIsValidDecoderOutput = ef->IsValidInputExtension(df->szOutputExtension);
                if (bIsValidDecoderOutput == false)
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140006));
                    return false;
                }

                szDecInputFile = szEncInputFile;

                std::wstring szPath = util::Utilities::GetFilePath(szEncOutputFile);
                std::wstring szName = util::Utilities::GenerateUuidString();
                std::wstring szExt = util::string::TowLower(df->szOutputExtension);
                szDecOutputFile = szPath + szName + L"." + szExt;
            }

            if (bCanEncode == false)
            {
                dcl.Build(df, df->nDefaultPreset, item.nId, szDecInputFile, szDecOutputFile, df->bPipeInput, df->bPipeOutput, L"");
            }

            std::wstring szInputFile = bCanEncode == true ? szEncInputFile : szDecOutputFile;

            ecl.Build(ef, item.nPreset, item.nId, szInputFile, szEncOutputFile, ef->bPipeInput, ef->bPipeOutput, item.szOptions);

            bool bDecoderPipes = dcl.bUseReadPipes && dcl.bUseWritePipes;
            bool bEncoderPipes = ecl.bUseReadPipes && ecl.bUseWritePipes;
            bool bCanTranscode = !bCanEncode && bDecoderPipes && bEncoderPipes;

            if (bCanTranscode)
            {
                // trans-code
                try
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000C));
                    item.ResetProgress();

                    bool bResult = ConvertFileUsingOnlyPipes(ctx, dcl, ecl, m_down);
                    if (bResult == true)
                    {
                        if (config->m_Options.bDeleteSourceFiles == true)
                            util::Utilities::DeleteFile(szEncInputFile);

                        return true;
                    }
                    else
                    {
                        if (config->m_Options.bDeleteOnErrors == true)
                            util::Utilities::DeleteFile(szEncOutputFile);

                        return false;
                    }
                }
                catch (...)
                {
                    if (config->m_Options.bDeleteOnErrors == true)
                        util::Utilities::DeleteFile(szEncOutputFile);

                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000E));
                    ctx->ItemProgress(item.nId, -1, true, true);
                }

                return false;
            }

            // decode
            if (bCanEncode == false)
            {
                try
                {
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140007));
                    item.ResetProgress();

                    bool bResult = false;
                    if ((dcl.bUseReadPipes == false) && (dcl.bUseWritePipes == false))
                        bResult = ConvertFileUsingConsole(ctx, dcl, m_down);
                    else
                        bResult = ConvertFileUsingPipes(ctx, dcl, m_down);
                    if (bResult == false)
                    {
                        if (config->m_Options.bDeleteOnErrors == true)
                            util::Utilities::DeleteFile(szDecOutputFile);

                        return false;
                    }

                    if (util::Utilities::FileExists(szDecOutputFile) == false)
                    {
                        ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140008));
                        return false;
                    }
                }
                catch (...)
                {
                    if (config->m_Options.bDeleteOnErrors == true)
                        util::Utilities::DeleteFile(szEncOutputFile);

                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x00140009));
                    ctx->ItemProgress(item.nId, -1, true, true);
                }
            }

            if (ctx->bRunning == false)
                return false;

            // encode
            try
            {
                if (ef->nType == config::FormatType::Encoder)
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000A));
                else if (ef->nType == config::FormatType::Decoder)
                    ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000B));

                item.ResetProgress();

                bool bResult = false;
                if ((ecl.bUseReadPipes == false) && (ecl.bUseWritePipes == false))
                    bResult = ConvertFileUsingConsole(ctx, ecl, m_down);
                else
                    bResult = ConvertFileUsingPipes(ctx, ecl, m_down);
                if (bResult == true)
                {
                    if (bCanEncode == false)
                        util::Utilities::DeleteFile(szDecOutputFile);

                    if (config->m_Options.bDeleteSourceFiles == true)
                        util::Utilities::DeleteFile(szEncInputFile);

                    return true;
                }
                else
                {
                    if (bCanEncode == false)
                        util::Utilities::DeleteFile(szDecOutputFile);

                    if (config->m_Options.bDeleteOnErrors == true)
                        util::Utilities::DeleteFile(szEncOutputFile);

                    return false;
                }
            }
            catch (...)
            {
                if (bCanEncode == false)
                    util::Utilities::DeleteFile(szDecOutputFile);

                if (config->m_Options.bDeleteOnErrors == true)
                    util::Utilities::DeleteFile(szEncOutputFile);

                ctx->ItemStatus(item.nId, ctx->GetString(0x00150001), ctx->GetString(0x0014000E));
                ctx->ItemProgress(item.nId, -1, true, true);
            }

            return false;
        }
        bool CWorker::ConvertLoop(IWorkerContext* ctx, std::queue<int> &queue, std::mutex &m_queue, std::mutex &m_dir, std::mutex &m_down)
        {
            auto config = ctx->pConfig;
            while (true)
            {
                try
                {
                    m_queue.lock();
                    if (!queue.empty())
                    {
                        int nId = queue.front();
                        queue.pop();
                        m_queue.unlock();

                        if (ctx->bRunning == false)
                            return false;

                        ctx->TotalProgress(nId);

                        if (ConvertItem(ctx, config->m_Items[nId], m_dir, m_down) == true)
                        {
                            ctx->nProcessedFiles++;
                            ctx->TotalProgress(nId);
                        }
                        else
                        {
                            ctx->nProcessedFiles++;
                            ctx->nErrors++;
                            ctx->TotalProgress(nId);
                            if (config->m_Options.bStopOnErrors == true)
                                return false;
                        }

                        if (ctx->bRunning == false)
                            return false;
                    }
                    else
                    {
                        m_queue.unlock();
                        return true;
                    }
                }
                catch (...)
                {
                    return false;
                }
            }
            return true;
        }
        void CWorker::Convert(IWorkerContext* ctx, config::CItem& item)
        {
            std::mutex m_dir;
            std::mutex m_down;

            ctx->Start();
            ctx->nTotalFiles = 1;
            ctx->TotalProgress(item.nId);

            if (ConvertItem(ctx, item, m_dir, m_down) == true)
            {
                ctx->nProcessedFiles = 1;
                ctx->nErrors = 0;
                ctx->TotalProgress(item.nId);
            }
            else
            {
                ctx->nProcessedFiles = 1;
                ctx->nErrors = 1;
                ctx->TotalProgress(item.nId);
            }

            ctx->Stop();
            ctx->bDone = true;
        }
        void CWorker::Convert(IWorkerContext* ctx, std::vector<config::CItem>& items)
        {
            std::queue<int> queue;
            std::mutex m_queue;
            std::mutex m_dir;
            std::mutex m_down;

            ctx->Start();

            for (auto& item : items)
            {
                if (item.bChecked == true)
                {
                    item.ResetProgress();
                    queue.push(item.nId);
                    ctx->nTotalFiles++;
                }
            }

            if (ctx->nThreadCount <= 1)
            {
                this->ConvertLoop(ctx, queue, m_queue, m_dir, m_down);
            }
            else
            {
                auto convert = [&]() { this->ConvertLoop(ctx, queue, m_queue, m_dir, m_down); };
                auto threads = std::make_unique<std::thread[]>(ctx->nThreadCount);

                for (int i = 0; i < ctx->nThreadCount; i++)
                    threads[i] = std::thread(convert);

                for (int i = 0; i < ctx->nThreadCount; i++)
                    threads[i].join();
            }

            ctx->Stop();
            ctx->bDone = true;
        }
    };
}
