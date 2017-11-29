/*
Copyright (c) 2011, Jason Lloyd-Price, Abhishekh Gupta, and Andre S. Ribeiro
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * The names of the contributors may not be used to endorse or promote
	  products derived from this software without specific prior written
	  permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* parsestream.h/cpp

sgns2::parse::ParseStream class contents:
	- Character-by-character stream with preprocessing
	- Keeps track of line number and position in the line for error messages
	- Strips comments automatically
	- Can change the 'eof' character
*/

#ifndef PARSESTREAM_H
#define PARSESTREAM_H

#include <string>
#include <iostream>

namespace sgns2 {
namespace parse {

// ParseStream provides nice helper functions to the parser
// It also acts as the preprocessor, removing comments and
// processing #directives

class ParseStream {
public:
	ParseStream( std::istream &in, const char *src );
	~ParseStream();

	// Get a character
	int get();
	// Get a character, ignoring leading spaces
	int sget();
	// Peek at the next character
	int peek();
	// Peek at the next non-whitespace character
	int speek();

	// Check for eof
	inline bool eof() { return peek() < 0; }

	// Put a character back
	void putback( char ch ) { backbuf[backbuflen++] = ch; }
	// Strip leading whitespace. Returns -1 on EOF
	int strip() { return speek() >= 0 ? 0 : -1; }

	// Set false EOF characters
	inline void setEOFOn( char eof ) { eofChar = eof; secondEofChar = -666; }
	inline void setSecondEOF( char eof2 ) { secondEofChar = eof2; }
	// Clear false EOF characters
	int clearEOF();

	// Get the current line number
	int getLineNo();
	int getLineChar();
	inline const char *getSource() { return source.c_str(); }
	// Get the current line
	// NOTE: Currently destroys the parsing state. get() and related
	//       functions should not be called after calling this.
	const char *getCurLine();
	
	// Ignore until an end condition
	int ignore( const char *endCond );
	// Reads a lua block - matches ( with ) and [ with ]
	// Returns true on ending with the end char, false on EOF
	bool readLua( std::ostream &out, char end = '\0' );

private:
	ParseStream &operator =( const ParseStream& );

	// Input stream
	std::istream &in;

	// Putback buffer
	char backbuf[32];
	int backbuflen;

	// False EOF
	int eofChar, secondEofChar;
	
	// Current source
	char curLine[84];
	int curLineBoundary;
	int lineno;
	int charPos;
	std::string source;

	// Is the cursor at the line start
	bool atLineStart;
};

// ---------------------------------------------------------------------------
// Whitespace character classifier
bool inline charIsWhitespace (char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// ---------------------------------------------------------------------------
// Alphabet character classifier
bool inline charIsAlpha (char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// ---------------------------------------------------------------------------
// C-style alphabet character classifier
bool inline charIsAlphaC (char c) {
	return charIsAlpha (c) || c == '_';
}

// ---------------------------------------------------------------------------
// Digit character classifier
bool inline charIsDigit (char c) {
	return c >= '0' && c <= '9';
}

// ---------------------------------------------------------------------------
// C-style alphanumeric character classifier
bool inline charIsAlnumC (char c) {
	return charIsAlphaC (c) || charIsDigit (c);
}

} // namespace parse
} // namespace sgns2

#endif //PARSESTREAM_H
