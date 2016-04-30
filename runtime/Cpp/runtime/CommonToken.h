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

#pragma once

#include "WritableToken.h"

namespace org {
namespace antlr {
namespace v4 {
namespace runtime {

  class CommonToken : public WritableToken {
  protected:
    static const std::pair<TokenSource*, CharStream*> EMPTY_SOURCE;

    int _type;
    int _line;
    int _charPositionInLine; // set to invalid position
    int _channel;
    std::pair<TokenSource*, CharStream*> _source; // Pure references, usually from statically allocated classes.

    /// We need to be able to change the text once in a while.  If
    ///  this is non-empty, then getText should return this.  Note that
    ///  start/stop are not affected by changing this.
    ///
    // TO_DO: can store these in map in token stream rather than as field here
    std::wstring _text;

    /// <summary>
    /// What token number is this from 0..n-1 tokens; < 0 implies invalid index </summary>
    int _index;

    /// <summary>
    /// The char position into the input buffer where this token starts </summary>
    int _start;

    /// <summary>
    /// The char position into the input buffer where this token stops </summary>
    int _stop;

  public:
    CommonToken(int type);
    CommonToken(std::pair<TokenSource*, CharStream*> source, int type, int channel, int start, int stop);
    CommonToken(int type, const std::wstring &text);
    CommonToken(Token *oldToken);

    virtual int getType() const override;
    virtual void setLine(int line) override;
    virtual std::wstring getText() override;

    /// <summary>
    /// Override the text for this token.  getText() will return this text
    ///  rather than pulling from the buffer.  Note that this does not mean
    ///  that start/stop indexes are not valid.  It means that that input
    ///  was converted to a new string in the token object.
    /// </summary>
    virtual void setText(const std::wstring &text) override;
    virtual int getLine() override;

    virtual int getCharPositionInLine() override;
    virtual void setCharPositionInLine(int charPositionInLine) override;

    virtual int getChannel() override;
    virtual void setChannel(int channel) override;

    virtual void setType(int type) override;

    virtual int getStartIndex() override;
    virtual void setStartIndex(int start);

    virtual int getStopIndex() override;
    virtual void setStopIndex(int stop);

    virtual int getTokenIndex() override;
    virtual void setTokenIndex(int index) override;

    virtual TokenSource *getTokenSource() override;
    virtual CharStream *getInputStream() override;

  private:
    void InitializeInstanceFields();
  };

} // namespace runtime
} // namespace v4
} // namespace antlr
} // namespace org