
// See parsestream.h for a description of the contents of this file.

#include "stdafx.h"

#include <iostream>
#include <cstring>

#include "parsestream.h"

namespace sgns2 {
namespace parse {

const int DEFAULT_LINE_BOUNDARY = 70; // Longest line length to keep track of
const int CUT_LINE_TO = 25; // Length to cut the current line to when the line boundary is exceeded

// ---------------------------------------------------------------------------
ParseStream::ParseStream( std::istream &in, const char *src )
: in(in)
, backbuflen(0)
, eofChar(-666)
, secondEofChar(-666)
, curLineBoundary(DEFAULT_LINE_BOUNDARY)
, lineno(1)
, charPos(0)
, source(src)
, atLineStart(true)
{
	curLine[0] = '\0';
}

// ---------------------------------------------------------------------------
ParseStream::~ParseStream() {
}

// ---------------------------------------------------------------------------
int ParseStream::get() {
	int ch;

	if( backbuflen > 0 ) {
		// Empty the putback buffer first
		ch = backbuf[--backbuflen];
	} else {
		// Pull another character from the stream
		ch = in.get();
		switch( ch ) {
		case '/': {
				// Collapse comments into whitespace
				int cm = in.peek();
				if( cm == '/' ) {
					ignore( "\n" );
					ch = ' ';
				} else if( cm == '*' ) {
					in.get(); // /*/
					ignore( "*/" );
					ch = ' ';
				}
				curLine[charPos++] = ch;
				atLineStart = false;
			}
			break;
		case -1:
			if( atLineStart )
				break;
			// fall-through
		case '\n':
			lineno++;
			atLineStart = true;
			curLine[charPos] = '\0'; // End the current line
			charPos = 0;
			break;
		case '#':
			if( atLineStart ) {
				// TODO: Parse C preprocessor line and file
				// Currently, ignore lines starting with #
				ignore( "\n" );
				ch = ' ';
				break;
			}
			curLine[charPos++] = '#';
			break;
		case '\t':
			atLineStart = false;
			curLine[charPos++] = ' ';
			while( charPos & 3u )
				curLine[charPos++] = ' ';
			break;
		default:
			atLineStart = false;
			curLine[charPos++] = (char)ch;
			break;
		}
	
		if( charPos >= curLineBoundary ) {
			// Cut long lines for error messages
			// TODO: Optimize this stuff.. long lines will needlessly slow the parser down
			int cut = charPos - CUT_LINE_TO;
			memcpy( &curLine[0], &curLine[cut], CUT_LINE_TO );
			charPos = cut;
			memcpy( &curLine[0], "... ", 4 );
		}
	}

	// Pretend we hit the end if we hit the eofchar
	if( ch == eofChar && (secondEofChar < 0 || in.peek() == secondEofChar) ) {
		putback( eofChar );
		return -1;
	}

	return ch;
}

// ---------------------------------------------------------------------------
int ParseStream::sget() {
	int ch;
	do {
		ch = get();
	} while( ch >= 0 && charIsWhitespace( (char)ch) );
	return ch;
}

// ---------------------------------------------------------------------------
int ParseStream::peek() {
	int ch = get();
	if( ch >= 0 )
		putback( (char)ch );
	return ch;
}

// ---------------------------------------------------------------------------
int ParseStream::speek() {
	int ch = sget();
	if( ch >= 0 )
		putback( (char)ch );
	return ch;
}

// ---------------------------------------------------------------------------
int ParseStream::clearEOF() {
	eofChar = -666;
	secondEofChar = -666;
	return (peek() < 0) ? -1 : 0;
}

// ---------------------------------------------------------------------------
int ParseStream::getLineNo() {
	if( atLineStart && lineno > 1 )
		return lineno - 1;
	return lineno;
}

// ---------------------------------------------------------------------------
int ParseStream::getLineChar() {
	if( atLineStart )
		return static_cast<int>(strlen( curLine ));
	return charPos - backbuflen;
}

// ---------------------------------------------------------------------------
const char *ParseStream::getCurLine() {
	if( !atLineStart ) {
		int ch;
		if( clearEOF() >= 0 ) {
			curLineBoundary = 77;
			while( true ) {
				if( charPos >= 76 ) {
					curLine[charPos] = '\0';
					charPos = 0;
					break;
				}
				ch = get();
				if( ch < 0 || ch == '\n' )
					break;
			}
			curLineBoundary = DEFAULT_LINE_BOUNDARY;
		}
	}
	return curLine;
}

// ---------------------------------------------------------------------------
int ParseStream::ignore( const char *endCond ) {
	while( true ) {
		const char *end = endCond;
		int ch = in.get();

		if( ch == '\n' ) {
			lineno++; // Count line numbers in comments
			atLineStart = true;
			curLine[charPos] = '\0'; // End the current line
			charPos = 0;
		}

		if( ch < 0 ) {
			return -1;
		} else if( ch == *end ) {
			do {
				end++;
				if( !(*end) ) return 0;
				ch = in.get();
				if( ch < 0 ) return -1;
			} while( ch == *end );
			in.putback( (char)ch );
		}
	}
}

// ---------------------------------------------------------------------------
bool ParseStream::readLua( std::ostream &out, char end ) {
	int oldEofChar = eofChar, oldSecondEofChar = secondEofChar;
	if( end )
		clearEOF();
	
	// Track nesting - ignore the difference between ( and [ - lua will do that for us
	int nestDepth = 0;
	bool inString = false;
	bool escaped = false;
	int stringStart = 0;

	int ch;
	while( (ch = get()) >= 0 ) {
		if( nestDepth == 0 && ch == end )
			break; // Done!

		if( ch == '(' || ch == '[' ) {
			if( !inString )
				nestDepth++;
		} else if( ch == ')' || ch == ']' ) {
			if( !inString && nestDepth > 0 )
				nestDepth--;
		} else if( ch == '\"' || ch == '\'' ) {
			if( inString ) {
				inString = !(ch == stringStart) || escaped;
			} else {
				stringStart = ch;
				inString = true;
			}
		}

		if( ch == '\\' )
			escaped = !escaped;
		else
			escaped = false;

		out.put( (char)ch );
	}

	if( ch >= 0 )
		putback( (char)ch );

	// Re-instate the EOF chars
	if( end && oldEofChar >= 0 ) {
		setEOFOn( (char)oldEofChar );
		if( oldSecondEofChar >= 0 )
			setSecondEOF( (char)oldSecondEofChar );
	}

	return ch >= 0;
}

} // namespace parse
} // namespace sgns2
