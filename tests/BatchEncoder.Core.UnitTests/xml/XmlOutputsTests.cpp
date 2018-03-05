﻿// Copyright (c) Wiesław Šoltés. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "stdafx.h"
#include "CppUnitTest.h"
#include "xml\XmlConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace BatchEncoderCoreUnitTests
{
    TEST_CLASS(XmlOutputs_Tests)
    {
    public:
        TEST_METHOD(XmlOutputs_Constructor)
        {
            #pragma warning(push)
            #pragma warning(disable:4101)
            xml::XmlDocumnent doc;
            xml::XmlOutputs xml(doc);
            #pragma warning(pop)
        }
    };
}