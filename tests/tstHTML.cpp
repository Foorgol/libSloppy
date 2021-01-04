/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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
#include <sstream>

#include <gtest/gtest.h>

#include "../Sloppy/HTML/StyledElement.h"

using namespace Sloppy::HTML;

TEST(HTML, StyledElement_Basics)
{
  StyledElement e{"abc"};
  ASSERT_EQ("<abc></abc>", e.to_html());

  e.addPlainText("xyz");
  ASSERT_EQ("<abc>xyz</abc>", e.to_html());

  e.addClass("c1");
  ASSERT_EQ("<abc class=\"c1\">xyz</abc>", e.to_html());

  e.addClass("c2");
  ASSERT_EQ("<abc class=\"c1 c2\">xyz</abc>", e.to_html());

  e.addStyle("s1", "v1");
  ASSERT_EQ("<abc style=\"s1: v1;\" class=\"c1 c2\">xyz</abc>", e.to_html());

  e.addStyle("s2", "v2");
  ASSERT_EQ("<abc style=\"s1: v1; s2: v2;\" class=\"c1 c2\">xyz</abc>", e.to_html());

  e.addAttr("a1", "av1");
  e.addAttr("a2", "av2");
  ASSERT_EQ("<abc a2=\"av2\" class=\"c1 c2\" a1=\"av1\" style=\"s1: v1; s2: v2;\">xyz</abc>", e.to_html());

  e.addAttr("a2", "av2New");
  ASSERT_EQ("<abc a2=\"av2New\" class=\"c1 c2\" a1=\"av1\" style=\"s1: v1; s2: v2;\">xyz</abc>", e.to_html());
}

//----------------------------------------------------------------------------

TEST(HTML, StyledElement_Nesting)
{
  StyledElement e1{"abc"};
  e1.addPlainText("plain1");
  StyledElement* e2 = e1.createContentChild("child");
  ASSERT_TRUE(e2 != nullptr);
  ASSERT_EQ("<abc>plain1<child></child></abc>", e1.to_html());

  e1.addPlainText("plain2");
  ASSERT_EQ("<abc>plain1<child></child>plain2</abc>", e1.to_html());

  StyledElement* e3 = e1.createContentChild("other");
  ASSERT_TRUE(e3 != nullptr);
  ASSERT_EQ("<abc>plain1<child></child>plain2<other></other></abc>", e1.to_html());

  StyledElement* e4 = e2->createContentChild("inner");
  ASSERT_TRUE(e4 != nullptr);
  ASSERT_EQ("<abc>plain1<child><inner></inner></child>plain2<other></other></abc>", e1.to_html());
}

