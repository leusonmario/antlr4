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

#include "IntStream.h"
#include "OrderedATNConfigSet.h"
#include "Token.h"
#include "LexerNoViableAltException.h"
#include "RuleStopState.h"
#include "RuleTransition.h"
#include "SingletonPredictionContext.h"
#include "PredicateTransition.h"
#include "ActionTransition.h"
#include "Interval.h"
#include "DFA.h"
#include "Lexer.h"

#include "DFAState.h"
#include "LexerATNConfig.h"
#include "EmptyPredictionContext.h"

#include "LexerATNSimulator.h"

using namespace org::antlr::v4::runtime;
using namespace org::antlr::v4::runtime::atn;
using namespace antlrcpp;

void LexerATNSimulator::SimState::reset() {
  index = -1;
  line = 0;
  charPos = -1;
  dfaState = nullptr; // Don't delete. It's just a reference.
}

void LexerATNSimulator::SimState::InitializeInstanceFields() {
  index = -1;
  line = 0;
  charPos = -1;
}

int LexerATNSimulator::match_calls = 0;


LexerATNSimulator::LexerATNSimulator(const ATN &atn, std::vector<dfa::DFA> &decisionToDFA,
                                     std::shared_ptr<PredictionContextCache> sharedContextCache)
: LexerATNSimulator(nullptr, atn, decisionToDFA, sharedContextCache) {
}

LexerATNSimulator::LexerATNSimulator(Lexer *recog, const ATN &atn, std::vector<dfa::DFA> &decisionToDFA,
                                     std::shared_ptr<PredictionContextCache> sharedContextCache)
  : ATNSimulator(atn, sharedContextCache), _recog(recog), _decisionToDFA(decisionToDFA) {
  InitializeInstanceFields();
}

void LexerATNSimulator::copyState(LexerATNSimulator *simulator) {
  _charPositionInLine = simulator->_charPositionInLine;
  _line = simulator->_line;
  _mode = simulator->_mode;
  _startIndex = simulator->_startIndex;
}

int LexerATNSimulator::match(CharStream *input, size_t mode) {
  match_calls++;
  _mode = mode;
  ssize_t mark = input->mark();

  auto onExit = finally([&] {
    input->release(mark);
  });

  _startIndex = (int)input->index();
  prevAccept.reset();
  const dfa::DFA &dfa = _decisionToDFA[mode];
  if (dfa.s0 == nullptr) {
    return matchATN(input);
  } else {
    return execATN(input, dfa.s0);
  }

  return -1;
}

void LexerATNSimulator::reset() {
  prevAccept.reset();
  _startIndex = 0; // Originally -1, but that would require a signed type with many casts.
                   // The initial value is never tested, so it doesn't matter which value is set here.
  _line = 1;
  _charPositionInLine = 0;
  _mode = Lexer::DEFAULT_MODE;
}

int LexerATNSimulator::matchATN(CharStream *input) {
  ATNState *startState = (ATNState *)atn.modeToStartState[_mode];

  if (debug) {
    std::wcout << L"matchATN mode" << _mode << L" start: " << startState << std::endl;
  }

  size_t old_mode = _mode;

  std::shared_ptr<ATNConfigSet> s0_closure = computeStartState(input, startState);
  bool suppressEdge = s0_closure->hasSemanticContext;
  s0_closure->hasSemanticContext = false;

  dfa::DFAState *next = addDFAState(s0_closure);
  if (!suppressEdge) {
    _decisionToDFA[_mode].s0 = next;
  }

  int predict = execATN(input, next);

  if (debug) {
    std::wcout << L"DFA after matchATN: " << _decisionToDFA[old_mode].toLexerString() << std::endl;
  }

  return predict;
}

int LexerATNSimulator::execATN(CharStream *input, dfa::DFAState *ds0) {
  //System.out.println("enter exec index "+input.index()+" from "+ds0.configs);
  if (debug) {
    std::wcout << L"start state closure=" << ds0->configs << std::endl;
  }

  ssize_t t = input->LA(1);
  dfa::DFAState *s = ds0; // s is current/from DFA state

  while (true) { // while more work
    if (debug) {
      std::wcout << L"execATN loop starting closure: " << s->configs << std::endl;
    }

    // As we move src->trg, src->trg, we keep track of the previous trg to
    // avoid looking up the DFA state again, which is expensive.
    // If the previous target was already part of the DFA, we might
    // be able to avoid doing a reach operation upon t. If s!=null,
    // it means that semantic predicates didn't prevent us from
    // creating a DFA state. Once we know s!=null, we check to see if
    // the DFA state has an edge already for t. If so, we can just reuse
    // it's configuration set; there's no point in re-computing it.
    // This is kind of like doing DFA simulation within the ATN
    // simulation because DFA simulation is really just a way to avoid
    // computing reach/closure sets. Technically, once we know that
    // we have a previously added DFA state, we could jump over to
    // the DFA simulator. But, that would mean popping back and forth
    // a lot and making things more complicated algorithmically.
    // This optimization makes a lot of sense for loops within DFA.
    // A character will take us back to an existing DFA state
    // that already has lots of edges out of it. e.g., .* in comments.
    dfa::DFAState *target = getExistingTargetState(s, t);
    if (target == nullptr) {
      target = computeTargetState(input, s, t);
    }

    if (target == ERROR.get()) {
      break;
    }

    if (target->isAcceptState) {
      captureSimState(input, target);
      if (t == EOF) {
        break;
      }
    }

    if (t != EOF) {
      consume(input);
      t = input->LA(1);
    }

    s = target; // flip; current DFA target becomes new src/from state
  }

  return failOrAccept(input, s->configs, t);
}

dfa::DFAState *LexerATNSimulator::getExistingTargetState(dfa::DFAState *s, ssize_t t) {
  if (s->edges.size() == 0 || t < MIN_DFA_EDGE || t > MAX_DFA_EDGE) {
    return nullptr;
  }

  dfa::DFAState *target = s->edges[(size_t)(t - MIN_DFA_EDGE)];
  if (debug && target != nullptr) {
    std::wcout << std::wstring(L"reuse state ") << s->stateNumber << std::wstring(L" edge to ") << target->stateNumber << std::endl;
  }

  return target;
}

dfa::DFAState *LexerATNSimulator::computeTargetState(CharStream *input, dfa::DFAState *s, ssize_t t) {
  std::shared_ptr<OrderedATNConfigSet> reach = std::make_shared<OrderedATNConfigSet>();

  // if we don't find an existing DFA state
  // Fill reach starting from closure, following t transitions
  getReachableConfigSet(input, s->configs, reach, t);

  if (reach->isEmpty()) { // we got nowhere on t from s
                          // we got nowhere on t, don't throw out this knowledge; it'd
                          // cause a failover from DFA later.
    addDFAEdge(s, t, ERROR.get());
    // stop when we can't match any more char
    return ERROR.get();
  }

  // Add an edge from s to target DFA found/created for reach
  return addDFAEdge(s, t, reach);
}

int LexerATNSimulator::failOrAccept(CharStream *input, std::shared_ptr<ATNConfigSet> reach, ssize_t t) {
  if (prevAccept.dfaState != nullptr) {
    int ruleIndex = prevAccept.dfaState->lexerRuleIndex;
    int actionIndex = prevAccept.dfaState->lexerActionIndex;
    accept(input, ruleIndex, actionIndex, (size_t)prevAccept.index, prevAccept.line, (size_t)prevAccept.charPos);
    return prevAccept.dfaState->prediction;
  } else {
    // if no accept and EOF is first char, return EOF
    if (t == EOF && input->index() == (size_t)_startIndex) {
      return EOF;
    }

    throw LexerNoViableAltException(_recog, input, (size_t)_startIndex, reach);
  }
}

void LexerATNSimulator::getReachableConfigSet(CharStream *input, std::shared_ptr<ATNConfigSet> closure,
                                              std::shared_ptr<ATNConfigSet> reach, ssize_t t) {
  // this is used to skip processing for configs which have a lower priority
  // than a config that already reached an accept state for the same rule
  int skipAlt = ATN::INVALID_ALT_NUMBER;

  for (auto c : closure->configs) {
    bool currentAltReachedAcceptState = c->alt == skipAlt;
    if (currentAltReachedAcceptState && (std::static_pointer_cast<LexerATNConfig>(c))->hasPassedThroughNonGreedyDecision()) {
      continue;
    }

    if (debug) {
      std::wcout << L"testing " << getTokenName((int)t) << " at " << c->toString(true) << std::endl;
    }

    size_t n = c->state->getNumberOfTransitions();
    for (size_t ti = 0; ti < n; ti++) { // for each transition
      Transition *trans = c->state->transition(ti);
      ATNState *target = getReachableTarget(trans, (int)t);
      if (target != nullptr) {
        if (this->closure(input, std::make_shared<LexerATNConfig>(std::static_pointer_cast<LexerATNConfig>(c), target),
                          reach, currentAltReachedAcceptState, true)) {
          // any remaining configs for this alt have a lower priority than
          // the one that just reached an accept state.
          skipAlt = c->alt;
          break;
        }
      }
    }
  }
}

void LexerATNSimulator::accept(CharStream *input, int ruleIndex, int actionIndex, size_t index, size_t line, size_t charPos) {
  if (debug) {
    std::wcout << L"ACTION ";
    if (_recog != nullptr) {
      std::wcout << _recog->getRuleNames()[(size_t)ruleIndex];
    } else {
      std::wcout << ruleIndex;
    }
    std::wcout << ":" << actionIndex << std::endl;
  }

  if (actionIndex >= 0 && _recog != nullptr) {
    _recog->action(nullptr, ruleIndex, actionIndex);
  }

  // seek to after last char in token
  input->seek(index);
  _line = line;
  _charPositionInLine = (int)charPos;
  if (input->LA(1) != EOF) {
    consume(input);
  }
}

atn::ATNState *LexerATNSimulator::getReachableTarget(Transition *trans, ssize_t t) {
  if (trans->matches((int)t, WCHAR_MIN, WCHAR_MAX)) {
    return trans->target;
  }

  return nullptr;
}

std::shared_ptr<ATNConfigSet> LexerATNSimulator::computeStartState(CharStream *input, ATNState *p) {
  std::shared_ptr<PredictionContext> initialContext  = PredictionContext::EMPTY; // ml: the purpose of this assignment is unclear
  std::shared_ptr<ATNConfigSet> configs = std::make_shared<OrderedATNConfigSet>();
  for (size_t i = 0; i < p->getNumberOfTransitions(); i++) {
    ATNState *target = p->transition(i)->target;
    LexerATNConfig::Ref c = std::make_shared<LexerATNConfig>(target, (int)(i + 1), initialContext);
    closure(input, c, configs, false, false);
  }
  return configs;
}

bool LexerATNSimulator::closure(CharStream *input, LexerATNConfig::Ref config, std::shared_ptr<ATNConfigSet> configs,
                                bool currentAltReachedAcceptState, bool speculative) {
  if (debug) {
    std::wcout << L"closure(" << config->toString(true) << L")" << std::endl;
  }

  if (is<RuleStopState *>(config->state)) {
    if (debug) {
      if (_recog != nullptr) {
        std::wcout << L"closure at " << _recog->getRuleNames()[(size_t)config->state->ruleIndex] << L" rule stop " << config << std::endl;
      } else {
        std::wcout << L"closure at rule stop " << config << std::endl;
      }
    }

    if (config->context == nullptr || config->context->hasEmptyPath()) {
      if (config->context == nullptr || config->context->isEmpty()) {
        configs->add(config);
        return true;
      } else {
        configs->add(std::make_shared<LexerATNConfig>(config, config->state, PredictionContext::EMPTY));
        currentAltReachedAcceptState = true;
      }
    }

    if (config->context != nullptr && !config->context->isEmpty()) {
      for (size_t i = 0; i < config->context->size(); i++) {
        if (config->context->getReturnState(i) != PredictionContext::EMPTY_RETURN_STATE) {
          std::weak_ptr<PredictionContext> newContext = config->context->getParent(i); // "pop" return state
          ATNState *returnState = atn.states[(size_t)config->context->getReturnState(i)];
          LexerATNConfig::Ref c = std::make_shared<LexerATNConfig>(returnState, config->alt, newContext.lock());
          currentAltReachedAcceptState = closure(input, c, configs, currentAltReachedAcceptState, speculative);
        }
      }
    }

    return currentAltReachedAcceptState;
  }

  // optimization
  if (!config->state->onlyHasEpsilonTransitions()) {
    if (!currentAltReachedAcceptState || !config->hasPassedThroughNonGreedyDecision()) {
      configs->add(config);
    }
  }

  ATNState *p = config->state;
  for (size_t i = 0; i < p->getNumberOfTransitions(); i++) {
    Transition *t = p->transition(i);
    LexerATNConfig::Ref c = getEpsilonTarget(input, config, t, configs, speculative);
    if (c != nullptr) {
      currentAltReachedAcceptState = closure(input, c, configs, currentAltReachedAcceptState, speculative);
    }
  }

  return currentAltReachedAcceptState;
}

LexerATNConfig::Ref LexerATNSimulator::getEpsilonTarget(CharStream *input, LexerATNConfig::Ref config, Transition *t,
                                                        std::shared_ptr<ATNConfigSet> configs, bool speculative) {
  LexerATNConfig::Ref c = nullptr;
  switch (t->getSerializationType()) {
    case Transition::RULE: {
      RuleTransition *ruleTransition = static_cast<RuleTransition*>(t);
      PredictionContext::Ref newContext = SingletonPredictionContext::create(config->context, ruleTransition->followState->stateNumber);
      c = std::make_shared<LexerATNConfig>(config, t->target, newContext);
    }
      break;

    case Transition::PRECEDENCE:
      throw UnsupportedOperationException("Precedence predicates are not supported in lexers.");

    case Transition::PREDICATE: {
      /*  Track traversing semantic predicates. If we traverse,
       we cannot add a DFA state for this "reach" computation
       because the DFA would not test the predicate again in the
       future. Rather than creating collections of semantic predicates
       like v3 and testing them on prediction, v4 will test them on the
       fly all the time using the ATN not the DFA. This is slower but
       semantically it's not used that often. One of the key elements to
       this predicate mechanism is not adding DFA states that see
       predicates immediately afterwards in the ATN. For example,

       a : ID {p1}? | ID {p2}? ;

       should create the start state for rule 'a' (to save start state
       competition), but should not create target of ID state. The
       collection of ATN states the following ID references includes
       states reached by traversing predicates. Since this is when we
       test them, we cannot cash the DFA state target of ID.
       */
      PredicateTransition *pt = static_cast<PredicateTransition*>(t);
      if (debug) {
        std::wcout << L"EVAL rule " << pt->ruleIndex << L":" << pt->predIndex << std::endl;
      }
      configs->hasSemanticContext = true;
      if (evaluatePredicate(input, pt->ruleIndex, pt->predIndex, speculative)) {
        c = std::make_shared<LexerATNConfig>(config, t->target);
      }
    }
      break;
      // ignore actions; just exec one per rule upon accept
    case Transition::ACTION:
      c = std::make_shared<LexerATNConfig>(config, t->target, (static_cast<ActionTransition*>(t))->actionIndex);
      break;
    case Transition::EPSILON:
      c = std::make_shared<LexerATNConfig>(config, t->target);
      break;
  }

  return c;
}

bool LexerATNSimulator::evaluatePredicate(CharStream *input, int ruleIndex, int predIndex, bool speculative) {
  // assume true if no recognizer was provided
  if (_recog == nullptr) {
    return true;
  }

  if (!speculative) {
    return _recog->sempred(nullptr, ruleIndex, predIndex);
  }

  int savedCharPositionInLine = _charPositionInLine;
  size_t savedLine = _line;
  size_t index = input->index();
  ssize_t marker = input->mark();

  auto onExit = finally([&] {
    _charPositionInLine = savedCharPositionInLine;
    _line = savedLine;
    input->seek(index);
    input->release(marker);
  });

  consume(input);
  return _recog->sempred(nullptr, ruleIndex, predIndex);
}

void LexerATNSimulator::captureSimState(CharStream *input, dfa::DFAState *dfaState) {
  prevAccept.index = (int)input->index();
  prevAccept.line = _line;
  prevAccept.charPos = _charPositionInLine;
  prevAccept.dfaState = dfaState;
}

dfa::DFAState *LexerATNSimulator::addDFAEdge(dfa::DFAState *from, ssize_t t, std::shared_ptr<ATNConfigSet> q) {
  /* leading to this call, ATNConfigSet.hasSemanticContext is used as a
   * marker indicating dynamic predicate evaluation makes this edge
   * dependent on the specific input sequence, so the static edge in the
   * DFA should be omitted. The target DFAState is still created since
   * execATN has the ability to resynchronize with the DFA state cache
   * following the predicate evaluation step.
   *
   * TJP notes: next time through the DFA, we see a pred again and eval.
   * If that gets us to a previously created (but dangling) DFA
   * state, we can continue in pure DFA mode from there.
   */
  bool suppressEdge = q->hasSemanticContext;
  q->hasSemanticContext = false;

  dfa::DFAState *to = addDFAState(q);

  if (suppressEdge) {
    return to;
  }

  addDFAEdge(from, t, to);
  return to;
}

void LexerATNSimulator::addDFAEdge(dfa::DFAState *p, ssize_t t, dfa::DFAState *q) {
  if (t < MIN_DFA_EDGE || t > MAX_DFA_EDGE) {
    // Only track edges within the DFA bounds
    return;
  }

  if (debug) {
    std::wcout << std::wstring(L"EDGE ") << p << std::wstring(L" -> ") << q << std::wstring(L" upon ") << (static_cast<wchar_t>(t)) << std::endl;
  }

  std::lock_guard<std::mutex> lck(mtx);
  if (p->edges.empty()) {
    //  make room for tokens 1..n and -1 masquerading as index 0
    p->edges.resize(MAX_DFA_EDGE - MIN_DFA_EDGE + 1);
  }
  p->edges[(size_t)(t - MIN_DFA_EDGE)] = q; // connect
}

dfa::DFAState *LexerATNSimulator::addDFAState(std::shared_ptr<ATNConfigSet> configs) {
  /* the lexer evaluates predicates on-the-fly; by this point configs
   * should not contain any configurations with unevaluated predicates.
   */
  assert(!configs->hasSemanticContext);

  dfa::DFAState *proposed = new dfa::DFAState(configs); /* mem-check: managed by the DFA or deleted below */
  ATNConfig::Ref firstConfigWithRuleStopState = nullptr;
  for (auto c : configs->configs) {
    if (is<RuleStopState*>(c->state)) {
      firstConfigWithRuleStopState = c;
      break;
    }
  }

  if (firstConfigWithRuleStopState != nullptr) {
    proposed->isAcceptState = true;
    proposed->lexerRuleIndex = firstConfigWithRuleStopState->state->ruleIndex;
    proposed->lexerActionIndex = (std::static_pointer_cast<LexerATNConfig>(firstConfigWithRuleStopState))->lexerActionIndex;
    proposed->prediction = atn.ruleToTokenType[(size_t)proposed->lexerRuleIndex];
  }

  dfa::DFA &dfa = _decisionToDFA[_mode];

  {
    std::lock_guard<std::mutex> lck(mtx);
    
    auto iterator = dfa.states.find(proposed);
    if (iterator != dfa.states.end()) {
      delete proposed;
      return iterator->second;
    }

    dfa::DFAState *newState = proposed;

    newState->stateNumber = (int)dfa.states.size();
    configs->setReadonly(true);
    newState->configs = configs;
    dfa.states[newState] = newState;
    return newState;
  }
}

dfa::DFA& LexerATNSimulator::getDFA(size_t mode) {
  return _decisionToDFA[mode];
}

std::wstring LexerATNSimulator::getText(CharStream *input) {
  // index is first lookahead char, don't include.
  return input->getText(misc::Interval((int)_startIndex, (int)input->index() - 1));
}

size_t LexerATNSimulator::getLine() const {
  return _line;
}

void LexerATNSimulator::setLine(size_t line) {
  _line = line;
}

int LexerATNSimulator::getCharPositionInLine() {
  return _charPositionInLine;
}

void LexerATNSimulator::setCharPositionInLine(int charPositionInLine) {
  _charPositionInLine = charPositionInLine;
}

void LexerATNSimulator::consume(CharStream *input) {
  ssize_t curChar = input->LA(1);
  if (curChar == L'\n') {
    _line++;
    _charPositionInLine = 0;
  } else {
    _charPositionInLine++;
  }
  input->consume();
}

std::wstring LexerATNSimulator::getTokenName(int t) {
  if (t == -1) {
    return L"EOF";
  }
  //if ( atn.g!=null ) return atn.g.getTokenDisplayName(t);
  return std::wstring(L"'") + static_cast<wchar_t>(t) + std::wstring(L"'");
}

void LexerATNSimulator::InitializeInstanceFields() {
  _startIndex = -1;
  _line = 1;
  _charPositionInLine = 0;
  _mode = org::antlr::v4::runtime::Lexer::DEFAULT_MODE;
}