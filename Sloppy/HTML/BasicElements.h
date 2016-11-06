#ifndef SLOPPY__HTML__BASIC_ELEMENTS_H
#define SLOPPY__HTML__BASIC_ELEMENTS_H

#include <string>
#include <vector>
#include <unordered_map>

#include "StyledElement.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    class Span : public StyledElement
    {
    public:
      explicit Span(const string& content, Alignment horAlignment = Alignment::Default);
      virtual ~Span(){}
    };

    //----------------------------------------------------------------------------

    class Para : public Span
    {
    public:
      explicit Para(const string& content, Alignment horAlignment = Alignment::Default)
        :Span(content, horAlignment) { elemName = "p"; }
      virtual ~Para(){}
    };

    //----------------------------------------------------------------------------

    class Head : public Span
    {
    public:
      explicit Head(int lvl, const string& content, Alignment horAlignment = Alignment::Default)
        :Span(content, horAlignment) { elemName = "h" + to_string(lvl); }
      virtual ~Head(){}
    };

    //----------------------------------------------------------------------------

    class Anchor : public StyledElement
    {
    public:
      explicit Anchor(const string& url, const string& linkText="");
      virtual ~Anchor(){}
    };

    //----------------------------------------------------------------------------

    enum class FormMethod
    {
      Post,
      Get
    };

    class Form : public StyledElement
    {
    public:
      Form(const string& id, const string& targetUrl, FormMethod method = FormMethod::Post, const string& encType = "multipart/form-data");
      virtual ~Form(){}
    };
  }
}

#endif
