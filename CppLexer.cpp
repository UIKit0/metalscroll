/*
*	Copyright 2009 Griffin Software
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License. 
*/

#include "MetalScrollPCH.h"
#include "CppLexer.h"

/* C code produced by gperf version 3.0.3 */

#define TOTAL_KEYWORDS 86
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 16
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 157

static unsigned int hash(const wchar_t* str, unsigned int len)
{
	static unsigned char asso_values[] =
	{
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		50, 158,  10, 158,   0, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158,   0, 158,  65, 158,  20,
		20,  15,   0,   0,  20,  40,   0,  10,  40,   5,
		0,   5,  75,  50,  40,  90,  15,   5,  55,  50,
		30,  50, 158,  30,  25, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158
	};

	int hval = len;

	switch(hval)
	{
		default:
			hval += asso_values[(unsigned char)str[6]];
			/*FALLTHROUGH*/
		case 6:
		case 5:
		case 4:
		case 3:
			hval += asso_values[(unsigned char)str[2]+2];
			/*FALLTHROUGH*/
		case 2:
			hval += asso_values[(unsigned char)str[1]];
			break;
	}
	return hval;
}

bool IsCppKeyword(const wchar_t* str, unsigned int len)
{
	static const wchar_t* wordlist[] =
	{
		L"", L"",
		L"if",
		L"", L"", L"",
		L"inline",
		L"do",
		L"", L"",
		L"__m64",
		L"__m128",
		L"", L"", L"",
		L"union",
		L"__int8",
		L"__int16",
		L"__inline",
		L"void",
		L"",
		L"delete",
		L"__leave",
		L"for",
		L"",
		L"__asm",
		L"",
		L"__int64",
		L"unsigned",
		L"__alignof",
		L"__if_not_exists",
		L"public",
		L"__m128d",
		L"__assume",
		L"this",
		L"while",
		L"struct",
		L"__raise",
		L"", L"",
		L"throw",
		L"static",
		L"wchar_t",
		L"template",
		L"char",
		L"break",
		L"static_cast",
		L"__based",
		L"__forceinline",
		L"else",
		L"__fastcall",
		L"__if_exists",
		L"__m128i",
		L"volatile",
		L"enum",
		L"",
		L"friend",
		L"default",
		L"int",
		L"bool",
		L"__try",
		L"double",
		L"__cdecl",
		L"__uuidof",
		L"goto",
		L"class",
		L"switch",
		L"__int32",
		L"new",
		L"__finally",
		L"false",
		L"sizeof",
		L"private",
		L"try",
		L"case",
		L"short",
		L"return",
		L"",
		L"register",
		L"__stdcall",
		L"",
		L"reinterpret_cast",
		L"mutable",
		L"__except",
		L"long",
		L"const",
		L"signed",
		L"",
		L"operator",
		L"", L"",
		L"extern",
		L"",
		L"continue",
		L"true",
		L"float",
		L"",
		L"typedef",
		L"",
		L"__wchar_t",
		L"__declspec",
		L"",
		L"virtual",
		L"typename",
		L"",
		L"using",
		L"", L"", L"", L"",
		L"const_cast",
		L"", L"", L"",
		L"protected",
		L"", L"", L"",
		L"explicit",
		L"", L"", L"", L"", L"", L"",
		L"catch",
		L"", L"", L"", L"", L"", L"", L"", L"", L"",
		L"", L"", L"", L"", L"", L"", L"", L"", L"",
		L"namespace",
		L"", L"", L"", L"", L"", L"", L"", L"", L"",
		L"", L"", L"",
		L"dynamic_cast"
	};

	if(len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
	{
		int key = hash(str, len);

		if(key <= MAX_HASH_VALUE && key >= 0)
		{
			const wchar_t* keyword = wordlist[key];
			unsigned int keywordLen = wcslen(keyword);
			if( (keywordLen == len) && !memcmp(str, keyword, len*sizeof(wchar_t)) )
				return true;
		}
	}
	return false;
}

bool IsCppIdSeparator(wchar_t chr)
{
	if( (chr >= L'0') && (chr <= L'9') )
		return false;

	if( (chr >= L'A') && (chr <= L'Z') )
		return false;

	if( (chr >= L'a') && (chr <= L'z') )
		return false;

	if(chr == L'_')
		return false;

	return true;
}
