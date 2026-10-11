#include "SimCore/Json.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace HigataSim
{
	namespace
	{
		struct FParser
		{
			const std::string& S;
			size_t P = 0;
			std::string Error;

			void SkipWs()
			{
				while (P < S.size() && (S[P] == ' ' || S[P] == '\t' || S[P] == '\n' || S[P] == '\r'))
				{
					++P;
				}
			}

			bool Fail(const char* What)
			{
				if (Error.empty())
				{
					Error = std::string(What) + " at byte " + std::to_string(P);
				}
				return false;
			}

			static void AppendUtf8(std::string& Out, unsigned Cp)
			{
				if (Cp < 0x80)
				{
					Out += static_cast<char>(Cp);
				}
				else if (Cp < 0x800)
				{
					Out += static_cast<char>(0xC0 | (Cp >> 6));
					Out += static_cast<char>(0x80 | (Cp & 0x3F));
				}
				else if (Cp < 0x10000)
				{
					Out += static_cast<char>(0xE0 | (Cp >> 12));
					Out += static_cast<char>(0x80 | ((Cp >> 6) & 0x3F));
					Out += static_cast<char>(0x80 | (Cp & 0x3F));
				}
				else
				{
					Out += static_cast<char>(0xF0 | (Cp >> 18));
					Out += static_cast<char>(0x80 | ((Cp >> 12) & 0x3F));
					Out += static_cast<char>(0x80 | ((Cp >> 6) & 0x3F));
					Out += static_cast<char>(0x80 | (Cp & 0x3F));
				}
			}

			bool Hex4(unsigned& Out)
			{
				if (P + 4 > S.size())
				{
					return Fail("short \\u escape");
				}
				Out = 0;
				for (int I = 0; I < 4; ++I)
				{
					const char C = S[P++];
					Out <<= 4;
					if (C >= '0' && C <= '9') Out |= static_cast<unsigned>(C - '0');
					else if (C >= 'a' && C <= 'f') Out |= static_cast<unsigned>(C - 'a' + 10);
					else if (C >= 'A' && C <= 'F') Out |= static_cast<unsigned>(C - 'A' + 10);
					else return Fail("bad \\u escape");
				}
				return true;
			}

			bool ParseString(std::string& Out)
			{
				++P; // opening quote
				while (P < S.size())
				{
					const char C = S[P++];
					if (C == '"')
					{
						return true;
					}
					if (C != '\\')
					{
						Out += C;
						continue;
					}
					if (P >= S.size())
					{
						break;
					}
					const char E = S[P++];
					switch (E)
					{
					case '"': Out += '"'; break;
					case '\\': Out += '\\'; break;
					case '/': Out += '/'; break;
					case 'b': Out += '\b'; break;
					case 'f': Out += '\f'; break;
					case 'n': Out += '\n'; break;
					case 'r': Out += '\r'; break;
					case 't': Out += '\t'; break;
					case 'u':
					{
						unsigned Cp = 0;
						if (!Hex4(Cp)) return false;
						if (Cp >= 0xD800 && Cp < 0xDC00 && P + 6 <= S.size() && S[P] == '\\' && S[P + 1] == 'u')
						{
							P += 2;
							unsigned Lo = 0;
							if (!Hex4(Lo)) return false;
							Cp = 0x10000 + ((Cp - 0xD800) << 10) + (Lo - 0xDC00);
						}
						AppendUtf8(Out, Cp);
						break;
					}
					default: return Fail("bad escape");
					}
				}
				return Fail("unterminated string");
			}

			bool ParseValue(FJson& Out)
			{
				SkipWs();
				if (P >= S.size())
				{
					return Fail("unexpected end");
				}
				const char C = S[P];
				if (C == '{')
				{
					Out.Type = FJson::EType::Object;
					++P;
					SkipWs();
					if (P < S.size() && S[P] == '}')
					{
						++P;
						return true;
					}
					for (;;)
					{
						SkipWs();
						if (P >= S.size() || S[P] != '"') return Fail("expected key");
						std::string Key;
						if (!ParseString(Key)) return false;
						SkipWs();
						if (P >= S.size() || S[P] != ':') return Fail("expected ':'");
						++P;
						Out.Members.emplace_back(std::move(Key), FJson());
						if (!ParseValue(Out.Members.back().second)) return false;
						SkipWs();
						if (P < S.size() && S[P] == ',') { ++P; continue; }
						if (P < S.size() && S[P] == '}') { ++P; return true; }
						return Fail("expected ',' or '}'");
					}
				}
				if (C == '[')
				{
					Out.Type = FJson::EType::Array;
					++P;
					SkipWs();
					if (P < S.size() && S[P] == ']')
					{
						++P;
						return true;
					}
					for (;;)
					{
						Out.Items.emplace_back();
						if (!ParseValue(Out.Items.back())) return false;
						SkipWs();
						if (P < S.size() && S[P] == ',') { ++P; continue; }
						if (P < S.size() && S[P] == ']') { ++P; return true; }
						return Fail("expected ',' or ']'");
					}
				}
				if (C == '"')
				{
					Out.Type = FJson::EType::String;
					return ParseString(Out.String);
				}
				if (S.compare(P, 4, "true") == 0) { Out.Type = FJson::EType::Bool; Out.Bool = true; P += 4; return true; }
				if (S.compare(P, 5, "false") == 0) { Out.Type = FJson::EType::Bool; Out.Bool = false; P += 5; return true; }
				if (S.compare(P, 4, "null") == 0) { Out.Type = FJson::EType::Null; P += 4; return true; }
				const char* Begin = S.c_str() + P;
				char* End = nullptr;
				const double V = std::strtod(Begin, &End);
				if (End == Begin) return Fail("bad value");
				Out.Type = FJson::EType::Number;
				Out.Number = V;
				P += static_cast<size_t>(End - Begin);
				return true;
			}
		};
	}

	bool FJson::Parse(const std::string& Text, FJson& Out, std::string& Error)
	{
		FParser Parser{Text, 0, std::string()};
		Out = FJson();
		if (!Parser.ParseValue(Out))
		{
			Error = Parser.Error;
			return false;
		}
		Parser.SkipWs();
		if (Parser.P != Text.size())
		{
			Error = "trailing characters at byte " + std::to_string(Parser.P);
			return false;
		}
		return true;
	}

	bool FJson::ParseFile(const std::string& Path, FJson& Out, std::string& Error)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			Error = "cannot open " + Path;
			return false;
		}
		std::ostringstream Buf;
		Buf << In.rdbuf();
		std::string Text = Buf.str();
		if (Text.size() >= 3 && static_cast<unsigned char>(Text[0]) == 0xEF && static_cast<unsigned char>(Text[1]) == 0xBB
			&& static_cast<unsigned char>(Text[2]) == 0xBF)
		{
			Text.erase(0, 3);
		}
		if (!Parse(Text, Out, Error))
		{
			Error = Path + ": " + Error;
			return false;
		}
		return true;
	}

	const FJson* FJson::Get(const std::string& Key) const
	{
		if (Type != EType::Object) return nullptr;
		for (const auto& M : Members)
		{
			if (M.first == Key) return &M.second;
		}
		return nullptr;
	}

	double FJson::NumberOr(const std::string& Key, double Default) const
	{
		const FJson* V = Get(Key);
		return V && V->IsNumber() ? V->Number : Default;
	}

	std::string FJson::StringOr(const std::string& Key, const std::string& Default) const
	{
		const FJson* V = Get(Key);
		return V && V->IsString() ? V->String : Default;
	}

	bool FJson::BoolOr(const std::string& Key, bool Default) const
	{
		const FJson* V = Get(Key);
		return V && V->Type == EType::Bool ? V->Bool : Default;
	}

	std::vector<std::string> FJson::Strings(const std::string& Key) const
	{
		std::vector<std::string> Out;
		const FJson* V = Get(Key);
		if (V && V->IsArray())
		{
			for (const FJson& I : V->Items)
			{
				if (I.IsString()) Out.push_back(I.String);
			}
		}
		return Out;
	}
}
