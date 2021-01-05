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

#ifndef SLOPPY__TEMPLATE_SYSTEM_H
#define SLOPPY__TEMPLATE_SYSTEM_H

#include <stddef.h>                                    // for size_t
#include <istream>                                     // for istream
#include <memory>                                      // for allocator, uni...
#include <optional>                                    // for optional
#include <regex>                                       // for regex, regex_i...
#include <string>                                      // for string, basic_...
#include <tuple>                                       // for tuple
#include <unordered_map>                               // for unordered_map
#include <vector>                                      // for vector

#include "../ConfigFileParser/ConfigFileParser.h"      // for Parser
#include "../json_fwd.hpp"                             // for json
#include "../String.h"  // for estring, Strin...

namespace Sloppy
{
  namespace TemplateSystem
  {
    /** \brief An Enum that defines entry types for a template syntax tree
     */
    enum class SyntaxTreeItemType
    {
      Static,       ///< static text
      Variable,     ///< a variable
      ForLoop,      ///< a `for`-loop with the loop items in a subtree
      Condition,    ///< an 'if'-item
      IncludeCmd    ///< an 'include' command for inserting another template
    };

    /** \brief An Enum that contains an entry for each language token in the template syntax
     */
    enum class TokenType
    {
      Variable,   ///< a variable
      StartIf,    ///< an opening `if`-statement ("`if`")
      EndIf,      ///< a closing 'if'-statement ("`endif`")
      StartFor,   ///< an opening 'for'-statement ("`for`")
      EndFor,     ///< a closing 'for'-statement ("`endfor`")
      IncludeCmd  ///< an 'include'-statement ("`include`")
    };

    enum class SyntaxSectionType
    {
      Root,
      ForLoop,
      Condition
    };

    /** \brief An element in the syntax tree that formally describes a template's contents
     */
    struct SyntaxTreeItem
    {
      SyntaxTreeItemType t;   ///< the item type (static text, variable, ...)
      std::string varName;         ///< in case of variable items: the variable's name
      std::string listName;        ///< in case of 'for'-loops: the name of the list to loop over
      std::string staticText;      ///< in case of static text: the text itself
      bool invertCondition;   ///< in case if 'if'-conditions: indicates whether the condition shall be inverted (aka "not")

      size_t idxNextSibling;  ///< index of the next sibling at the same hierarchy level as this item
      size_t idxFirstChild;   ///< for items that start a subtree: the index of the first child item in the subtree
      size_t idxParent;       ///< for subtree-elements: the index of the parent item. The data is redundant, but makes tree raversal in both directions easier

      size_t idxFirstChar;    ///< index of the first character in the template source text that is represented by this tree item
      size_t idxLastChar;     ///< index of the last character in the template source text that is represented by this tree item
    };
    using SyntaxTreeItemList = std::vector<SyntaxTreeItem>;

    /** \brief A 'struct' for collecting/reporting syntax errors in the template text
     */
    struct SyntaxTreeError
    {
      size_t line;   ///< number of the text line that contains the error
      size_t idxFirstChar;   ///< index of the first character of the "offending" section
      size_t idxLastChar;   ///< index of the last character of the "offending" section
      std::string msg;   ///< a message describing the error

      /** \brief Ctor for a new syntax error with an `sregex_iterator` defining the offending text range
       */
      SyntaxTreeError(
          const std::sregex_iterator::value_type& tokenMatch,   ///< the regex match that triggerd the syntax error
          const std::string& _msg   ///< a message describing the syntax error
          )
        :SyntaxTreeError{_msg}
      {
        updatePosition(tokenMatch);
      }

      /** \brief Default ctor, defining an empty error message
       */
      SyntaxTreeError()
        :idxFirstChar{0}, idxLastChar{0}, msg{} {}

      /** \brief Ctor for a syntax error that is only defined by a message and not by a text range
       */
      explicit SyntaxTreeError(
          const std::string& _msg   ///< the error message
          )
        :idxFirstChar{0}, idxLastChar{0}, msg{_msg} {}

      /** \returns `true` if the SyntaxTreeError contains a non-empty error message
       */
      bool isError() { return (!(msg.empty())); }

      /** \brief Updates the text range data (first / last character) from a 'sregex_iterator'
       */
      void updatePosition(const std::sregex_iterator::value_type& tokenMatch)
      {
        idxFirstChar = tokenMatch.position();
        idxLastChar = idxFirstChar + tokenMatch.size() - 1;
      }

      /** \returns a human-readable string that describes the syntax error and that can be printed e.g., to `stderr`
       */
      std::string str()
      {
        estring s = "Template parsing error at position %1: ";
        s.arg(idxFirstChar);
        s += msg;
        return s;
      }
    };

    /** \brief A class that parses raw template text into a tree of syntax items that represent the inner structure of the template content
     */
    class SyntaxTree
    {
    public:
      static constexpr size_t InvalidIndex = (1 << 31);

      /** \brief Ctor for an empty tree; simply initiates some member variables
       */
      SyntaxTree();

      /** \brief Parses the raw template content into a syntax tree
       *
       * \note All previous tree items are erased, even if we return an error!
       *
       * \returns the first encountered syntax error or an empty error if everything was okay
       */
      SyntaxTreeError parse(const estring& s);

      /** \returns a copy of the current syntax tree
       */
      SyntaxTreeItemList getTree() const { return tree; }

      /** \returns a read-only reference to the current syntax tree (avoids copying of data)
       */
      const SyntaxTreeItemList& getTreeAsRef() const { return tree; }

      /** \returns a list of all referenced templates in the tree
       */
      StringList getIncludes() const;

    private:
      SyntaxTreeItemList tree;

      // store a few regex objects in the class although we need them only
      // in the recursive syntax parser. This avoids the expensive re-instantiation
      // of the regexs each time we enter the recursion loop
      std::regex reToken;
      std::regex reFor;
      std::regex reIf;
      std::regex reVar;
      std::regex reInclude;

      std::tuple<TokenType, bool> checkToken(const std::string& token) const;

      std::tuple<TokenType, SyntaxTreeError> doSyntaxCheck(const std::string& token, SyntaxSectionType secType) const;
      int isValid_endif(const std::string& token) const;
      int isValid_if(const std::string& token) const;
      int isValid_endfor(const std::string& token) const;
      int isValid_for(const std::string& token) const;
      int isValid_include(const std::string& token) const;
      bool isValid_var(const std::string& token) const;
    };

    //----------------------------------------------------------------------------

    /** \brief A class that stores and parses a (text-based) template containing
     * variables and a simple template language.
     *
     * A template can be created with content from a file on disk, a string
     * or an input stream.
     *
     * The template language is as follows:
     *
     * * `{{VarName}}` denotes a variable called `VarName`. During template instantiation
     *   it will be replaced with content from a JSON dictionary (key = `VarName').
     *
     * * A `for`-loop looks like `{{ for x : listName }} Here is a list entry: {{x}} {{endfor}}`.
     *   The identifier `listName` will be looked up in the JSON dictionary and should resolve
     *   into an array. Everthing between `{{ for ... }}` and `{{ endfor }}` will be repeated for
     *   each entry in the array. Inside the loop, the `x` (or whatever name you choose) resolves
     *   into the array item for the current iteration.
     *
     * * An `if` can be built as follows: `{{if condVar}} Conditional text {{endif}}`. `condVar`
     *   will be looked up in the JSON dictionary and should be parseable as a bool. The text
     *   between `{{if ...}}` and `{{endif}}` will only be rendered if the `condVar` resolves
     *   to `true`.
     *
     * * JSON objects are supported. If a variable resolves into a JSON object, you can
     *   access object attributes using classic dot notation. Example: `{{ person.name }}`.
     *   This also works with loops: '{{ for p : personList }} Name = {{ p.name }} {{endfor}}`
     *
     * * `for`-loops can be nested:
     * `{{ for p : personList }} Name = {{p.name}}, Siblings = {{ for s : p.siblings}}{{s.name}} {{endfor}}{{endfor}}
     *
     * * `if` can be inside `for` and vice versa.
     *
     * * You can include other files from the template directory like this: `{{ include /other/file/name.txt }}`. Circular
     *   dependencies are detected and an error is raised. All template names, even if starting with a leading '`/`'
     *   are interpreted relative to the template root dir.
     *
     * * Variables like `{{ ::varName }}` (two leading colons) are looked up in a list of strings. The list can be loaded
     *   from an ini-style file in which the section headers (e.g., `[de]`) denote translation languages.
     *   If, for instace, the current language is set to "de" then `{{ ::varName }}` will be replaced with the
     *   value of `varName` in the section `[de]` of the ini file.
     *
     * * Templates can be organized in subdirectories with translations for each language. Example: `/my/template.txt`,
     *   `/de/my/template.txt`, `/es/my/template.txt`, etc. If existing, the localized version such
     *   as `/de/my/template.txt' will be preferred over the default `/my/template.txt` if `/my/template.txt`
     *   is requested by the user.
     *
     * * Whitespaces between the curly brackets and their content don't matter. `{{var}}' is equal to `{{ var }}'
     *   or `{{ var}}`.
     *
     * * Variable names and tokens are case-sensitive.
     *
     * * Text lines that only contain a control statement such as `if`, `for`, `endfor` or `endif` will not
     *   show up in the output at all. Not even as an empty line / newline.
     *
     */
    class Template
    {
    public:
      /** \brief Ctor for filling the template from an input stream.
       *
       * The stream is read until EOF.
       *
       * \note You must call `parse()` for actually triggering the
       * interpretation of the template content. The separate call
       * enables users to better deal with template syntax errors.
       *
       */
      explicit Template(
          std::istream& inData   ///< the stream that delivers the template content
          );

      /** \brief Ctor from string content.
       *
       * \note You must call `parse()` for actually triggering the
       * interpretation of the template content. The separate call
       * enables users to better deal with template syntax errors.
       *
       */
      explicit Template(
          const std::string& inData   ///< the string containing the template content
          );

      /** \brief Static function that tries to read the template
       * from a file on disk.
       *
       * \returns `nullptr` if the file could not be read.
       */
      static std::unique_ptr<Template> fromFile(const std::string& fName);

      /** \brief Parses the template content; precondition for
       * actually using the template.
       *
       * No matter how many errors the template contains: this
       * method returns only first error it encounters during
       * parsing.
       *
       * \returns an instance of SyntaxTreeError that is empty if
       * parsing was successful.
       */
      SyntaxTreeError parse();

      /** \returns `true` if the template was successfully parsed and
       * `false` if the template has not yet been parsed or parsing failed.
       */
      bool isSyntaxOkay() { return syntaxOkay; }

      /** \returns a list of other templates that are referenced
       * via `include` statements.
       *
       * The template names are returned as written in the template. This
       * method does not check for their validity or existence. It also does
       * no translation or mapping of the default template name to the
       * localized template name.
       */
      StringList getIncludes() const;

      /** \returns a read-only reference to the list of all items in the internal syntax tree.
       */
      const SyntaxTreeItemList& getTreeAsRef() const { return st.getTreeAsRef(); }

    private:
      const std::string rawData;   ///< the raw template content as provided to the ctor
      SyntaxTree st;   ///< the syntax tree representing the template content
      bool syntaxOkay;   ///< a tag that indicates whether the template was parsed successfully or not (yet)
    };

    //----------------------------------------------------------------------------

    /** \brief A collection of templates (represented as file/directory hierarchy in a given root directory)
     * and the necessary functionality for parsing templates and applying substitution dictionaries to them.
     */
    class TemplateStore
    {
    public:
      /** \brief Ctor for a new template store.
       *
       * Read templates upon creation. If the underlying files on disk change
       * after this file has been instantiated, the changes WILL NOT take effect
       * until a TemplateStore object has been created.
       *
       * \throws std::invalid_argument if the provided path is invalid or empty or
       * if didn't contain any valid template file.
       */
      TemplateStore(
          const std::string& rootDir,      ///< the absolute or relative path to the directory that contains the templates
          const StringList& extList   ///< a whitelist of file extensions (case-sensitive, no leading dot) that indicate valid template files in the root dir
          );

      /** \brief Sets the language code (= subdirectory in the template root dir) for template retrieval
       *
       * If empty, the default template in the root dir will be used.
       */
      void setLang(
          const std::string& lang = ""   ///< the new default language for template look-ups
          ) { langCode = lang; }

      /** \brief Reads a list of (translation) strings from an ini file
       *
       * The sections of the ini file should represent language codes, e.g., `[en]`, `[de]`, ...
       *
       * The key-value pairs in each section map symbolic string names to their translations.
       *
       * The provided ini file will simply be read as it is. There's no checking whatsoever that
       * each provided translation section is complete, consistent, etc.
       *
       * A previously read string list will not be deleted/released if opening of the new
       * string list failed.
       *
       * \returns `true` if the ini file could be parsed, `false` otherwise
       */
      bool setStringlist(const std::string& slPath);

      /** \brief Looks up a template with a given name, applies substitutions from a dictionary to it and returns the result.
       *
       * The first attempt is to retrieve `<languageCode>/<providedTemplatePath>`. If that template file doesn't exist,
       * the location '<providedTemplatePath>' is tried next. If that
       * template also doesn't exist, an exception is raised.
       *
       * \throws std::invalid_argument if the requested template doesn't exist (neither "localized" with language prefix nor directly)
       *
       * \throws std::runtime_error (raised by `getTemplate_Recursive`) if a circular dependency between templates (via `include`-statements)
       * has been detected.
       *
       * \returns the content of the requested template with all
       * substitutions recursively applied (other referenced/included templates etc.)
       */
      std::string get(
          const std::string& tName,   ///< the name of the template to retrieve (e.g., `subDir/template.txt`)
          const nlohmann::json& dic   ///< a JSON dictionary with substitution strings
          );

      /** \brief Looks up a string in the list of string translations
       *
       * The string list has to be previously loaded using `setStringList`.
       *
       * We first try to look up the string in the string list section for
       * the currently selected language (e.g., `[en]`). If that look-up failed
       * we try to find the string in default section of the ini file (that is
       * the part of the ini file without section header).
       *
       * \returns the translation of the requested string or an empty `optional<string>`
       * if no string list was loaded or if the requested string does not exist.
       */
      std::optional<std::string> getString(
          const std::string& sName,   ///< the template to look up
          const std::string& langOverride=""   ///< overrides the currently selected language with a different language
          ) const;

    protected:
      /** \brief Determines the actual name of the template file to use for a look-up
       *
       * If a language code is set (using `setLang') and `<lang>/<docname>` exists,
       * that path is returned.
       *
       * If a language code is set and `<lang>/<docname>` **does not** exist, the
       * direct path <docname> is returned, provided that the file exists.
       *
       * Without defined language code, `<docname>` is returned, provided that the file exists.
       *
       * If we couldn't determine a localized or default template of the given name, an
       * empty string is returned.
       *
       * \returns the best match for a requested template with a preference on the localized template;
       * empty string if no match could be found.
       */
      std::string getLocalizedTemplateName(
          const std::string& docName   ///< the name of the template to retrieve
          ) const;

      /** \brief Recursively visits all elements of a template and apply all substitutions
       *
       * First, the effective template is determined as described for `getLocalizedTemplateName`.
       * Subsequently, all substitutions are applied to that template's syntax tree and the
       * result is returned.
       *
       * \throws std::invalid_argument if the requested template could not be found
       *
       * \throws std::runtime_error if a circular dependency between templates (via `include`-statements)
       * has been detected.
       *
       * \returns the content of the requested template with all
       * substitutions recursively applied (other referenced/included templates etc.)
       */
      std::string getTemplate_Recursive(
          const std::string& tName,   ///< the name of the template to retrieve
          const nlohmann::json& dic,   ///< a dictionary with substitutions to be applied to the template
          StringList& visitedTemplates   ///< a list to which all visited sub-templates are append; used for the detection of circular dependencies
          ) const;

      /** \brief Iterates over a given subtree of a templates syntax tree and applies substitutions
       *
       * This function does the actual work of replacing `include`, `if` and `for` with the
       * content defined by the substitution dictionary.
       *
       * \returns a string that represents the content of the subtree with all substitutions applied.
       */
      std::string getSyntaxSubtree(
          const SyntaxTreeItemList& tree,   ///< a reference to the tree we're working on
          size_t idxFirstItem,    ///< the index of first item in the tree that shall be processed
          const nlohmann::json& dic,   ///< the substitutions to apply
          std::unordered_map<std::string, const nlohmann::json&>& localScopeVars,   ///< in case of nested statements (e.g., `if` inside a `for`) this map contains the local variables and their value
          StringList& visitedTemplates   ///< a list to which all visited sub-templates are append; used for the detection of circular dependencies
          ) const;

      /** \brief Resolves a template variable to its effective value,
       * taking local variables etc. into account.
       *
       * Local variables take precedence over dictionary values.
       *
       * \throws std::runtime_error if an invalid variable name (e.g., "`.xy`") has been detected
       *
       * \returns a Json::Value with the resolved symbol
       */
      const nlohmann::json& resolveVariable(
          const std::string& varName,   ///< the name of the variable to resolve
          const nlohmann::json& dic,  ///< the dictionary for looking up variable values
          std::unordered_map<std::string, const nlohmann::json&>& localScopeVars   ///< a map of local variables
          ) const;

    private:
      std::unordered_map<std::string, Template> docs;   ///< a mapping of relative file names to Template instances
      std::string langCode;   ///< the current language code
      std::unique_ptr<Parser> strList;   ///< the currently load string list

    };
  }
}

#endif
