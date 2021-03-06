﻿/*
 * [The "BSD license"]
 *  Copyright (c) 2016 Mike Lischke
 * Copyright (c) 2013 Terence Parr
 * Copyright (c) 2013 Sam Harwell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tree/pattern/ParseTreePatternMatcher.h"
#include "tree/pattern/ParseTreeMatch.h"
//#include "XPath.h"

#include "tree/pattern/ParseTreePattern.h"

using namespace org::antlr::v4::runtime::tree;
using namespace org::antlr::v4::runtime::tree::pattern;

ParseTreePattern::ParseTreePattern(ParseTreePatternMatcher *matcher, const std::string &pattern, int patternRuleIndex,
  Ref<ParseTree> patternTree)
  : patternRuleIndex(patternRuleIndex), pattern(pattern), patternTree(patternTree), matcher(matcher) {
}

ParseTreeMatch ParseTreePattern::match(Ref<ParseTree> tree) {
  return matcher->match(tree, *this);
}

bool ParseTreePattern::matches(Ref<ParseTree> tree) {
  return matcher->match(tree, *this).succeeded();
}

/*
std::vector<ParseTreeMatch*> ParseTreePattern::findAll(ParseTree *tree, const std::string &xpath) {
  std::vector<ParseTree*> subtrees = xpath::XPath::findAll(tree, xpath, matcher->getParser());
  std::vector<ParseTreeMatch*> matches = std::vector<ParseTreeMatch*>();
  for (auto t : subtrees) {
    ParseTreeMatch *aMatch = match(t);
    if (aMatch->succeeded()) {
      matches.push_back(aMatch);
    }
  }
  return matches;
}
*/

ParseTreePatternMatcher *ParseTreePattern::getMatcher() const {
  return matcher;
}

std::string ParseTreePattern::getPattern() const {
  return pattern;
}

int ParseTreePattern::getPatternRuleIndex() const {
  return patternRuleIndex;
}

Ref<ParseTree> ParseTreePattern::getPatternTree() const {
  return patternTree;
}
