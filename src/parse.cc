/*
 Copyright (c) 2019 Fredrik Öhrström

 MIT License

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include "parse.h"
#include "util.h"
#include <string.h>
#include <stdarg.h>
#include "rapidxml/rapidxml.hpp"

using namespace rapidxml;
using namespace std;

/*

  The xmq format is very easy to parse:

  You got the character delimiters {}=()' and whitespace that are reserved
  for the xmq format.

  Then you have text which is any text string containing none of the reserved characters.

  Then you have quoted text that is surrounded with ', and is allowed to contain
  the reserved characters.

  Data is text or quoted text.

*/

bool isWhiteSpace(char c);

struct Parser
{
    const char *file;

    char *s;
    int line;
    int col;
    xml_document<> *doc;
    xml_node<> *root;

    void error(const char* fmt, ...);

    TokenType tokenType(char c);
    bool isTokenIdentifier(char c);
    void eatWhiteSpace();
    TokenType peekToken();
    Token eatToken();

    Token eatToEndOfLine();
    Token eatToEndOfText();
    Token eatToEndOfQuotedText();
    Token eatToEndOfData();

    void parseNode(xml_node<> *parent);
    void parseNodeContent(xml_node<> *parent);
    void parseAttributes(xml_node<> *parent);
};

void Parser::error(const char* fmt, ...)
{
    printf("%s:%d:%d: error: ", file, line, col);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    printf("%.*s\n", col, s-col+1);
    exit(1);
}

void trimTokenWhiteSpace(Token *t)
{
    // Trim away whitespace at the beginning.
    while (t->len > 0 && *t->data != 0)
    {
        if (!isWhiteSpace(*t->data)) break;
        t->data++;
    }

    // Trim away whitespace at the end.
    while (t->len >= 1)
    {
        if (!isWhiteSpace(t->data[t->len-1])) break;
        t->len--;
    }
}

TokenType Parser::tokenType(char c)
{
    switch (c)
    {
    case 0: return TokenType::none;
    case '\'': return TokenType::quote;
    case '=': return TokenType::equals;
    case '{': return TokenType::brace_open;
    case '}': return TokenType::brace_close;
    case '(': return TokenType::paren_open;
    case ')': return TokenType::paren_close;
    case '/': return TokenType::comment;
    default: return TokenType::text;
    }
}


bool Parser::isTokenIdentifier(char c)
{
    return
        c == 0 ||
        c == '#' ||
        c == '\'' ||
        c == '=' ||
        c == '{' ||
        c == '}' ||
        c == '(' ||
        c == ')' ||
        c == '/';
}


const char *tokenTypeText(TokenType t)
{
    switch (t)
    {
    case TokenType::none: return "end of file";
    case TokenType::quote: return "quoted text";
    case TokenType::equals: return "=";
    case TokenType::brace_open: return "{";
    case TokenType::brace_close: return "}";
    case TokenType::paren_open: return "(";
    case TokenType::paren_close: return ")";
    case TokenType::comment: return "/";
    case TokenType::text: return "text";
    }
    assert(0);
}

void Parser::eatWhiteSpace()
{
    while (true)
    {
        if (*s == 0) break;
        else
        if (isNewLine(*s))
        {
            col = 1;
            line++;
        }
        else
        if (!isWhiteSpace(*s)) break;
        // Eat whitespace...
        s++;
        col++;
    }
}

TokenType Parser::peekToken()
{
    while (true)
    {
        if (*s == 0) break;
        else
        if (isNewLine(*s))
        {
            col = 1;
            line++;
        }
        else
        if (!isWhiteSpace(*s)) break;
        // Eat whitespace...
        s++;
        col++;
    }
    return tokenType(*s);
}

Token Parser::eatToken()
{
    TokenType tt = peekToken();
    switch (tt)
    {
    case TokenType::none: return Token(TokenType::none, "", 0);
    case TokenType::text: return eatToEndOfText();
    case TokenType::quote: return eatToEndOfQuotedText();
    case TokenType::comment:
    {
        s++;
        if (*s != '*' && *s != '/')
        {
            error("expected // or /* for comment");
        }
        s++;
        Token t = eatToEndOfLine();
        trimTokenWhiteSpace(&t);
        return t;
    }
    case TokenType::equals:
    case TokenType::brace_open:
    case TokenType::brace_close:
    case TokenType::paren_open:
    case TokenType::paren_close:
        const char *tok = s;
        s++;
        return Token(tt, tok, 1);
    }
    return Token(TokenType::none, "", 0);
}

Token Parser::eatToEndOfText()
{
    char *start = s;
    char *p = start;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            s = p;
            break;
        }
        if (c == '\n')
        {
            s = p+1;
            line++;
            col = 1;
            break;
        }
        if (isTokenIdentifier(c) ||
            isWhiteSpace(c))
        {
            s = p;
            break;
        }
        p++;
        col++;
    }
    return Token(TokenType::text, start, p-start);
}

Token Parser::eatToEndOfQuotedText()
{
    char *start = s + 1;
    char *p = start;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            error("unexpected eof in quoted text");
        }
        if (c == '\n')
        {
            line++;
            col = 1;
        }
        if (c == '\'')
        {
            s = p+1;
            break;
        }
        p++;
    }
    return Token(TokenType::text, start, p-start);
}

Token Parser::eatToEndOfLine()
{
    char *start = s;
    char *p = start;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            s = p;
            break;
        }
        if (c == '\n')
        {
            s = p+1;
            line++;
            col = 1;
            break;
        }
        p++;
        col++;
    }
    return Token(TokenType::text, start, p-start);
}

void Parser::parseNodeContent(xml_node<> *parent)
{
    eatToken();
    while (true)
    {
        TokenType t = peekToken();

        if (t == TokenType::comment)
        {
            Token val = eatToken();
            char *value = doc->allocate_string(val.data, val.len+1);
            strncpy(value, val.data, val.len);
            value[val.len] = 0;
            parent->append_node(doc->allocate_node(node_comment, NULL, value));
        }
        else
        if (t == TokenType::text)
        {
            parseNode(parent);
        }
        else
        if (t == TokenType::quote)
        {
            Token val = eatToken();
            char *value = doc->allocate_string(val.data, val.len+1);
            strncpy(value, val.data, val.len);
            value[val.len] = 0;
            parent->append_node(doc->allocate_node(node_data, NULL, value));
        }
        else
        if (t == TokenType::brace_close ||
            t == TokenType::none)
        {
            eatToken();
            break;
        }
        else
        {
            error("unexpected token");
        }
    }
}

void Parser::parseAttributes(xml_node<> *parent)
{
    Token po = eatToken();
    assert(po.type == TokenType::paren_open);

    while (true)
    {
        Token t = eatToken();
        if (t.type == TokenType::paren_close) break;
        if (t.type != TokenType::text)
        {
            error("expected attribute");
        }
        TokenType nt = peekToken();
        if (nt == TokenType::text ||
            nt == TokenType::paren_close)
        {
            // This attribute is completed, it has no data.
            char *name = doc->allocate_string(t.data, t.len+1);
            strncpy(name, t.data, t.len);
            name[t.len] = 0;
            parent->append_attribute(doc->allocate_attribute(name,"true"));
            continue;
        }

        if (peekToken() != TokenType::equals) error("expected =");
        eatToken();

        Token val = eatToken();
        if (val.type == TokenType::text ||
            val.type == TokenType::quote)
        {
            char *name = doc->allocate_string(t.data, t.len+1);
            strncpy(name, t.data, t.len);
            name[t.len] = 0;
            char *value = doc->allocate_string(val.data, val.len+1);
            strncpy(value, val.data, val.len);
            value[val.len] = 0;
            parent->append_attribute(doc->allocate_attribute(name, value));
        }
    }

}


void Parser::parseNode(xml_node<> *parent)
{
    Token t = eatToken();
    if (t.type != TokenType::text) error("expected tag");

    const char *name = doc->allocate_string(t.data, t.len);
    xml_node<> *node = doc->allocate_node(node_element, name);
    parent->append_node(node);

    TokenType tt = peekToken();

    if (tt == TokenType::paren_open)
    {
        parseAttributes(node);
        tt = peekToken();
    }

    if (tt == TokenType::brace_open)
    {
        parseNodeContent(node);
    }
    else if (tt == TokenType::equals)
    {
        eatToken();
        Token val = eatToken();
        if (val.type != TokenType::text &&
            val.type != TokenType::quote)
        {
            error("expected text or quote");
        }
        char *value = doc->allocate_string(val.data, val.len+1);
        strncpy(value, val.data, val.len);
        value[val.len] = 0;
        node->append_node(doc->allocate_node(node_data, NULL, value));
    }
}

void parse(const char *filename, char *xml, xml_document<> *doc)
{
    Parser parser;

    parser.doc = doc;
    parser.s = xml;
    parser.line = 1;
    parser.col = 1;
    parser.file = filename;
    parser.parseNode(doc);

    if (TokenType::none != parser.peekToken())
    {
        printf("Syntax error, no more data is allowed after last closing brace.\n");
        exit(1);
    }
}