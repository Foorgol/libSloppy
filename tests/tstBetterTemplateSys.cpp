/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/TemplateProcessor/TemplateSys.h"
#include "BasicTestClass.h"

using namespace Sloppy::TemplateSystem;


TEST(BetterTemplateSys, ctor)
{
  // this should run without any problems
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};
  ts = TemplateStore{"../tests/sampleTemplateStore/", {"txt", "html"}};

  // point to invalid directory
  ASSERT_THROW(TemplateStore("/blabla", {"txt", "html"}), std::invalid_argument);

  // point to file instead of directory
  ASSERT_THROW(TemplateStore("../tests/sampleTemplateStore/t1.txt", {"txt", "html"}), std::invalid_argument);

  // filter out all files
  ASSERT_THROW(TemplateStore("../tests/sampleTemplateStore", {"xyz",}), std::invalid_argument);
}

//----------------------------------------------------------------------------
