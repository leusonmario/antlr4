﻿/*
 * [The "BSD license"]
 *  Copyright (c) 2016 Mike Lischke
 *  Copyright (c) 2013 Terence Parr
 *  Copyright (c) 2013 Dan McLaughlin
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ErrorNodeImpl.h"
#include "Interval.h"
#include "Parser.h"
#include "Token.h"
#include "CPPUtils.h"

#include "ParserRuleContext.h"

using namespace org::antlr::v4::runtime;
using namespace antlrcpp;

ParserRuleContext::ParserRuleContext() {
}

void ParserRuleContext::copyFrom(std::shared_ptr<ParserRuleContext> ctx) {
  // from RuleContext
  this->parent = ctx->parent;
  this->invokingState = ctx->invokingState;

  this->start = ctx->start;
  this->stop = ctx->stop;
}

ParserRuleContext::ParserRuleContext(std::weak_ptr<ParserRuleContext> parent, int invokingStateNumber)
  : RuleContext(parent, invokingStateNumber) {
}

void ParserRuleContext::enterRule(std::shared_ptr<tree::ParseTreeListener> listener) {
}

void ParserRuleContext::exitRule(std::shared_ptr<tree::ParseTreeListener> listener) {
}

std::shared_ptr<tree::TerminalNode> ParserRuleContext::addChild(std::shared_ptr<tree::TerminalNode> t) {
  children.push_back(t);
  return t;
}

RuleContext::Ref ParserRuleContext::addChild(RuleContext::Ref ruleInvocation) {
  children.push_back(ruleInvocation);
  return ruleInvocation;
}

void ParserRuleContext::removeLastChild() {
  if (!children.empty()) {
    children.pop_back();
  }
}

std::shared_ptr<tree::TerminalNode> ParserRuleContext::addChild(Token::Ref matchedToken) {
  std::shared_ptr<tree::TerminalNodeImpl> t = std::make_shared<tree::TerminalNodeImpl>(matchedToken);
  addChild(t);
  t->parent = shared_from_this();
  return t;
}

std::shared_ptr<tree::ErrorNode> ParserRuleContext::addErrorNode(Token::Ref badToken) {
  std::shared_ptr<tree::ErrorNodeImpl> t = std::make_shared<tree::ErrorNodeImpl>(badToken);
  addChild(t);
  t->parent = shared_from_this();
  return t;
}

std::shared_ptr<tree::Tree> ParserRuleContext::getChildReference(std::size_t i) {
  return children[i];
}

std::shared_ptr<tree::TerminalNode> ParserRuleContext::getToken(int ttype, std::size_t i) {
  if (i >= children.size()) {
    return nullptr;
  }

  size_t j = 0; // what token with ttype have we found?
  for (auto o : children) {
    if (is<tree::TerminalNode>(o)) {
      std::shared_ptr<tree::TerminalNode> tnode = std::dynamic_pointer_cast<tree::TerminalNode>(o);
      Token::Ref symbol = tnode->getSymbol();
      if (symbol->getType() == ttype) {
        if (j++ == i) {
          return tnode;
        }
      }
    }
  }

  return nullptr;
}

std::vector<std::shared_ptr<tree::TerminalNode>> ParserRuleContext::getTokens(int ttype) {
  std::vector<std::shared_ptr<tree::TerminalNode>> tokens;
  for (auto &o : children) {
    if (is<tree::TerminalNode>(o)) {
      std::shared_ptr<tree::TerminalNode> tnode = std::dynamic_pointer_cast<tree::TerminalNode>(o);
      Token::Ref symbol = tnode->getSymbol();
      if (symbol->getType() == ttype) {
        tokens.push_back(tnode);
      }
    }
  }

  return tokens;
}



std::size_t ParserRuleContext::getChildCount() {
  return children.size();
}

misc::Interval ParserRuleContext::getSourceInterval() {
  if (start == nullptr || stop == nullptr) {
    return misc::Interval::INVALID;
  }
  return misc::Interval(start->getTokenIndex(), stop->getTokenIndex());
}

Token::Ref ParserRuleContext::getStart() {
  return start;
}

Token::Ref ParserRuleContext::getStop() {
  return stop;
}

std::wstring ParserRuleContext::toInfoString(Parser *recognizer) {
  std::vector<std::wstring> rules = recognizer->getRuleInvocationStack(shared_from_this());
  std::reverse(rules.begin(), rules.end());
  std::wstring rulesStr = antlrcpp::arrayToString(rules);
  return std::wstring(L"ParserRuleContext") + rulesStr + std::wstring(L"{") + std::wstring(L"start=") +
    std::to_wstring(start->getTokenIndex())  + std::wstring(L", stop=") +
    std::to_wstring(stop->getTokenIndex()) + L'}';
}
