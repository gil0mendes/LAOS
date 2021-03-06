/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 <author>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 * @brief		x86 assembly code definitions.
 */

#ifndef __X86_ASM_H
#define __X86_ASM_H

#ifndef __ASM__
# error "What are you doing?"
#endif

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(_name)		\
	.global _name; \
	.type _name, @function; \
	_name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(_name)	\
	.type _name, @function; \
	_name:

/** Macro to define the end of a function. */
#define FUNCTION_END(_name)		\
	.size _name, . - _name

/** Macro to define a global symbol. */
#define SYMBOL(_name)			\
	.global _name; \
	_name:

/** Macro to define a global symbol with alignment. */
#define SYMBOL_ALIGNED(_name, _align)	\
	.align _align; \
	.global _name; \
	_name:

#endif /* __X86_ASM_H */
