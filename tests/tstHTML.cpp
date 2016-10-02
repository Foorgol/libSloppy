#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

#include "../Sloppy/HTML/StyledElement.h"

using namespace Sloppy::HTML;

TEST(HTML, StyledElement_Basics)
{
  StyledElement e{"abc"};
  ASSERT_EQ("<abc></abc>", e.to_html());

  e.setPlainTextContent("xyz");
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
  ASSERT_EQ("<abc a2=\"av2\" a1=\"av1\" style=\"s1: v1; s2: v2;\" class=\"c1 c2\">xyz</abc>", e.to_html());

  e.addAttr("a2", "av2New");
  ASSERT_EQ("<abc a2=\"av2New\" a1=\"av1\" style=\"s1: v1; s2: v2;\" class=\"c1 c2\">xyz</abc>", e.to_html());
}

//----------------------------------------------------------------------------

TEST(HTML, StyledElement_Nesting)
{
  StyledElement e1{"abc"};
  StyledElement* e2 = e1.createContentChild("child");
  ASSERT_TRUE(e2 != nullptr);
  ASSERT_EQ("<abc><child></child></abc>", e1.to_html());

  StyledElement* e3 = e1.createContentChild("other");
  ASSERT_TRUE(e3 != nullptr);
  ASSERT_EQ("<abc><child></child><other></other></abc>", e1.to_html());

  StyledElement* e4 = e2->createContentChild("inner");
  ASSERT_TRUE(e4 != nullptr);
  ASSERT_EQ("<abc><child><inner></inner></child><other></other></abc>", e1.to_html());
}

