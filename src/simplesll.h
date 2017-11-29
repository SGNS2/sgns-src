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

/* simplesll.h

SimpleSLL class contents:
	- A very small, simple Singly-linked list for data that fits into a pointer

*/

#ifndef SIMPLESLL_H
#define SIMPLESLL_H

#include "mempool.h"

class SimpleSLL {
public:
	// ----------------------------------------------------------------------------
	SimpleSLL()
		: head(NULL), tail(NULL)
	{ }
		
	// ----------------------------------------------------------------------------
	~SimpleSLL() {
		while( !empty() )
			pop_front<void*>();
	}

	// ----------------------------------------------------------------------------
	inline bool empty() const {
		return head == NULL;
	}

	// ----------------------------------------------------------------------------
	template< typename T >
	inline void push_front( T dat ) {
		Node *node = pool.alloc();
		node->dat = reinterpret_cast<void*>(dat);
		node->next = head;
		if( !head )
			tail = node;
		head = node;
	}

	// ----------------------------------------------------------------------------
	template< typename T >
	inline void push_back( T dat ) {
		Node *node = pool.alloc();
		node->dat = reinterpret_cast<void*>(dat);
		node->next = NULL;
		if( !head ) {
			head = node;
		} else {
			tail->next = node;
		}
		tail = node;
	}

	// ----------------------------------------------------------------------------
	template< typename T >
	inline T pop_front() {
		assert( head );
		T ret = reinterpret_cast<T>(head->dat);
		Node *next = head->next;
		pool.free( head );
		head = next;
		return ret;
	}

	// ----------------------------------------------------------------------------
	inline void pop_front() {
		pop_front<void*>();
	}

	// ----------------------------------------------------------------------------
	template< typename T >
	inline T front() {
		assert( head );
		return reinterpret_cast<T>(head->dat);
	}

private:
	struct Node {
		void *dat;
		Node *next;
	};

	Node *head, *tail;

	MemoryPool<Node> pool;
};

#endif // SIMPLESLL_H
